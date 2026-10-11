#include "mod-ollama-chat_scope.h"

#include "mod-ollama-chat_config.h"
#include "mod-ollama-chat_response.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace
{
    constexpr std::size_t MAX_SCOPE_KEYS = 512;
    constexpr uint32_t MAX_LINE_BYTES = 240;

    bool g_scopeHistoryEnabled = true;
    uint32_t g_scopeHistoryLines = 12;
    uint32_t g_scopeHistoryMaxAgeSeconds = 600;

    std::mutex g_scopeHistoryMutex;
    std::unordered_map<std::string, std::deque<ScopeLine>> g_scopeHistory;

    bool IsExpired(const ScopeLine& line, time_t now)
    {
        return std::difftime(now, line.at) > g_scopeHistoryMaxAgeSeconds;
    }

    void PruneScope(std::deque<ScopeLine>& lines, time_t now)
    {
        for (auto it = lines.begin(); it != lines.end();)
        {
            if (IsExpired(*it, now))
                it = lines.erase(it);
            else
                ++it;
        }

        while (lines.size() > g_scopeHistoryLines)
            lines.pop_front();
    }

    void PruneAll(time_t now)
    {
        for (auto it = g_scopeHistory.begin(); it != g_scopeHistory.end();)
        {
            PruneScope(it->second, now);
            if (it->second.empty())
                it = g_scopeHistory.erase(it);
            else
                ++it;
        }
    }

    void EvictOldestScope()
    {
        auto oldestScope = g_scopeHistory.end();
        time_t oldestLineAt = 0;
        bool foundOldest = false;

        for (auto scope = g_scopeHistory.begin(); scope != g_scopeHistory.end(); ++scope)
        {
            const auto oldestLine = std::min_element(
                scope->second.begin(), scope->second.end(),
                [](const ScopeLine& left, const ScopeLine& right)
                {
                    return left.at < right.at;
                });

            if (oldestLine != scope->second.end() &&
                (!foundOldest || oldestLine->at < oldestLineAt))
            {
                oldestScope = scope;
                oldestLineAt = oldestLine->at;
                foundOldest = true;
            }
        }

        if (oldestScope != g_scopeHistory.end())
            g_scopeHistory.erase(oldestScope);
    }
}

void ScopeHistory_Configure(bool enabled, uint32_t lines, uint32_t maxAgeSeconds)
{
    std::lock_guard<std::mutex> lock(g_scopeHistoryMutex);
    g_scopeHistoryEnabled = enabled;
    g_scopeHistoryLines = lines;
    g_scopeHistoryMaxAgeSeconds = maxAgeSeconds;
    PruneAll(std::time(nullptr));
}

void ScopeHistory_Record(const std::string& scopeKey, uint64_t speakerGuid,
                         const std::string& speaker, bool isBot,
                         const std::string& text)
{
    if (scopeKey.empty())
        return;

    const std::string boundedText = ClampReplyLength(text, MAX_LINE_BYTES);
    const time_t now = std::time(nullptr);

    std::lock_guard<std::mutex> lock(g_scopeHistoryMutex);
    if (!g_scopeHistoryEnabled || g_scopeHistoryLines == 0)
        return;

    PruneAll(now);

    auto scope = g_scopeHistory.find(scopeKey);
    if (scope == g_scopeHistory.end())
    {
        if (g_scopeHistory.size() >= MAX_SCOPE_KEYS)
            EvictOldestScope();

        scope = g_scopeHistory.try_emplace(scopeKey).first;
    }

    scope->second.push_back({speakerGuid, speaker, isBot, boundedText, now});
    while (scope->second.size() > g_scopeHistoryLines)
        scope->second.pop_front();
}

std::vector<ScopeLine> ScopeHistory_Get(const std::string& scopeKey)
{
    std::lock_guard<std::mutex> lock(g_scopeHistoryMutex);
    if (!g_scopeHistoryEnabled || scopeKey.empty())
        return {};

    auto scope = g_scopeHistory.find(scopeKey);
    if (scope == g_scopeHistory.end())
        return {};

    PruneScope(scope->second, std::time(nullptr));
    if (scope->second.empty())
    {
        g_scopeHistory.erase(scope);
        return {};
    }

    std::vector<ScopeLine> result(scope->second.begin(), scope->second.end());
    std::stable_sort(result.begin(), result.end(),
                     [](const ScopeLine& left, const ScopeLine& right)
                     {
                         return left.at < right.at;
                     });
    return result;
}

std::vector<OllamaChatTurn> ScopeHistory_BuildTurns(const std::string& scopeKey,
                                                    uint64_t botGuid,
                                                    uint64_t excludeLastFromGuid,
                                                    const std::string& excludeLastText)
{
    std::vector<ScopeLine> lines = ScopeHistory_Get(scopeKey);
    if (!lines.empty() && excludeLastFromGuid != 0 && !excludeLastText.empty())
    {
        const ScopeLine& last = lines.back();
        const bool exactMatch = last.text == excludeLastText;
        const bool truncatedPrefix = !last.text.empty() &&
            last.text.size() < excludeLastText.size() &&
            excludeLastText.compare(0, last.text.size(), last.text) == 0;
        if (last.speakerGuid == excludeLastFromGuid && (exactMatch || truncatedPrefix))
            lines.pop_back();
    }

    std::vector<OllamaChatTurn> turns;
    turns.reserve(lines.size());
    for (const ScopeLine& line : lines)
    {
        if (line.speakerGuid == botGuid)
            turns.push_back({ true, line.text });
        else
            turns.push_back({ false, line.speaker + ": " + line.text });
    }

    return turns;
}

std::string ScopeHistory_RenderText(const std::vector<OllamaChatTurn>& turns)
{
    if (turns.empty())
        return {};

    std::string text;
    if (!g_ScopeHistoryTextHeader.empty())
        text = g_ScopeHistoryTextHeader + "\n";

    for (const OllamaChatTurn& turn : turns)
    {
        if (turn.assistant)
            text += g_ScopeHistorySelfLabel + ": ";
        text += turn.content + "\n";
    }

    text += "\n";
    return text;
}

void ScopeHistory_Clear()
{
    std::lock_guard<std::mutex> lock(g_scopeHistoryMutex);
    g_scopeHistory.clear();
}
