#ifndef MOD_OLLAMA_CHAT_SCOPE_H
#define MOD_OLLAMA_CHAT_SCOPE_H

#include "mod-ollama-chat_api.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

struct ScopeLine
{
    uint64_t speakerGuid = 0;
    std::string speaker;
    bool isBot = false;
    std::string text;
    time_t at = 0;
};

void ScopeHistory_Configure(bool enabled, uint32_t lines, uint32_t maxAgeSeconds);
void ScopeHistory_Record(const std::string& scopeKey, uint64_t speakerGuid,
                         const std::string& speaker, bool isBot,
                         const std::string& text);
std::vector<ScopeLine> ScopeHistory_Get(const std::string& scopeKey);
std::vector<OllamaChatTurn> ScopeHistory_BuildTurns(const std::string& scopeKey,
                                                    uint64_t botGuid,
                                                    uint64_t excludeLastFromGuid,
                                                    const std::string& excludeLastText);
// World thread only: reads the configured header and self label for Ollama prompts.
std::string ScopeHistory_RenderText(const std::vector<OllamaChatTurn>& turns);
void ScopeHistory_Clear();

#endif // MOD_OLLAMA_CHAT_SCOPE_H
