#include "mod-ollama-chat_style.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    struct Abbreviation
    {
        std::string from;
        std::string to;
    };

    struct StyleSettings
    {
        bool enabled = false;
        uint32_t lowercaseStartChance = 40;
        uint32_t dropFinalPeriodChance = 60;
        uint32_t dropOpeningMarksChance = 80;
        uint32_t noAccentsChance = 20;
        uint32_t abbreviationsChance = 30;
        uint32_t typoPercent = 2;
        std::vector<Abbreviation> abbreviations;
    };

    std::mutex g_styleMutex;
    StyleSettings g_styleSettings;

    uint32_t ClampChance(uint32_t chance)
    {
        return std::min(chance, 100U);
    }

    std::string Trim(const std::string& value)
    {
        const size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};
        const size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    // These helpers only change the Spanish/Latin characters used by this
    // feature. All other UTF-8 bytes pass through as part of their sequence.
    std::string LowerUtf8(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (size_t i = 0; i < value.size();)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (c < 0x80)
            {
                result.push_back(static_cast<char>(std::tolower(c)));
                ++i;
                continue;
            }

            if (i + 1 < value.size())
            {
                const unsigned char second = static_cast<unsigned char>(value[i + 1]);
                if (c == 0xC3 && second >= 0x80 && second <= 0x9E && second != 0x97)
                {
                    result.push_back(static_cast<char>(0xC3));
                    result.push_back(static_cast<char>(second + 0x20));
                    i += 2;
                    continue;
                }
            }

            result.push_back(value[i++]);
        }
        return result;
    }

    bool IsLetterByte(unsigned char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
    }

    bool IsWordByte(unsigned char c)
    {
        return IsLetterByte(c) || (c >= '0' && c <= '9') || c == '_';
    }

    bool StartsUppercase(std::string_view word)
    {
        if (word.empty())
            return false;
        const unsigned char c = static_cast<unsigned char>(word.front());
        if (c >= 'A' && c <= 'Z')
            return true;
        if (word.size() >= 2 && c == 0xC3)
        {
            const unsigned char second = static_cast<unsigned char>(word[1]);
            return (second >= 0x80 && second <= 0x96) || (second >= 0x98 && second <= 0x9E);
        }
        return false;
    }

    std::string LowerFirstLetter(std::string_view word)
    {
        std::string result(word);
        if (result.empty())
            return result;

        unsigned char c = static_cast<unsigned char>(result[0]);
        if (c >= 'A' && c <= 'Z')
        {
            result[0] = static_cast<char>(c + ('a' - 'A'));
            return result;
        }
        if (result.size() >= 2 && c == 0xC3)
        {
            unsigned char second = static_cast<unsigned char>(result[1]);
            if ((second >= 0x80 && second <= 0x96) || (second >= 0x98 && second <= 0x9E))
                result[1] = static_cast<char>(second + 0x20);
        }
        return result;
    }

    std::string RemoveAccents(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (size_t i = 0; i < value.size();)
        {
            if (i + 1 < value.size() && static_cast<unsigned char>(value[i]) == 0xC3)
            {
                const unsigned char second = static_cast<unsigned char>(value[i + 1]);
                char replacement = 0;
                switch (second)
                {
                    case 0xA1: replacement = 'a'; break; // á
                    case 0xA9: replacement = 'e'; break; // é
                    case 0xAD: replacement = 'i'; break; // í
                    case 0xB3: replacement = 'o'; break; // ó
                    case 0xBA: replacement = 'u'; break; // ú
                    case 0xBC: replacement = 'u'; break; // ü
                    case 0x81: replacement = 'A'; break; // Á
                    case 0x89: replacement = 'E'; break; // É
                    case 0x8D: replacement = 'I'; break; // Í
                    case 0x93: replacement = 'O'; break; // Ó
                    case 0x9A: replacement = 'U'; break; // Ú
                    case 0x9C: replacement = 'U'; break; // Ü
                    default: break;
                }
                if (replacement)
                {
                    result.push_back(replacement);
                    i += 2;
                    continue;
                }
            }
            result.push_back(value[i++]);
        }
        return result;
    }

    uint64_t FeatureHash(uint64_t botGuid, std::string_view feature)
    {
        uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : feature)
        {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        hash ^= botGuid;
        hash *= 1099511628211ULL;
        hash ^= botGuid >> 32;
        hash *= 1099511628211ULL;
        return hash;
    }

    bool HasTrait(uint64_t botGuid, std::string_view feature, uint32_t chance)
    {
        return chance > 0 && FeatureHash(botGuid, feature) % 100 < chance;
    }

    bool ApplyThisMessage()
    {
        thread_local std::mt19937 generator(std::random_device{}());
        return std::uniform_int_distribution<uint32_t>(1, 100)(generator) <= 85;
    }

    uint32_t RandomInclusive(uint32_t min, uint32_t max)
    {
        thread_local std::mt19937 generator(std::random_device{}());
        return std::uniform_int_distribution<uint32_t>(min, max)(generator);
    }

    struct Word
    {
        size_t begin;
        size_t end;
        size_t ordinal;
        bool protectedToken;
    };

    bool IsProtectedRange(const std::vector<bool>& protectedBytes, size_t begin, size_t end);

    std::vector<bool> FindProtectedBytes(const std::string& text)
    {
        std::vector<bool> protectedBytes(text.size(), false);
        auto protect = [&](size_t begin, size_t end)
        {
            for (size_t i = begin; i < end && i < protectedBytes.size(); ++i)
                protectedBytes[i] = true;
        };

        // WoW hyperlinks contain spaces in their visible [item name] text.
        // Protect the whole hyperlink so a later word pass cannot alter it.
        for (size_t i = 0; i + 1 < text.size(); ++i)
        {
            if (text[i] != '|' || text[i + 1] != 'H')
                continue;
            size_t firstLabelEnd = text.find("|h", i + 2);
            if (firstLabelEnd == std::string::npos)
                continue;
            size_t secondLabelEnd = text.find("|h", firstLabelEnd + 2);
            if (secondLabelEnd == std::string::npos)
                continue;
            size_t end = secondLabelEnd + 2;
            if (text.compare(end, 2, "|r") == 0)
                end += 2;
            protect(i, end);
            i = end - 1;
        }

        // Bracketed link labels and other bracketed tokens are always opaque.
        for (size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] != '[')
                continue;
            size_t end = text.find(']', i + 1);
            if (end != std::string::npos)
            {
                protect(i, end + 1);
                i = end;
            }
        }

        // A whitespace-delimited token carrying markup, digits or @ is copied
        // verbatim, as required for links, colors, commands and identifiers.
        size_t tokenBegin = 0;
        while (tokenBegin < text.size())
        {
            size_t tokenEnd = text.find_first_of(" \t\r\n", tokenBegin);
            if (tokenEnd == std::string::npos)
                tokenEnd = text.size();
            bool protectToken = false;
            for (size_t i = tokenBegin; i < tokenEnd; ++i)
            {
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c == '|' || c == '[' || c == ']' || c == '@' || (c >= '0' && c <= '9'))
                {
                    protectToken = true;
                    break;
                }
            }
            if (protectToken)
                protect(tokenBegin, tokenEnd);
            tokenBegin = tokenEnd + (tokenEnd < text.size() ? 1 : 0);
        }

        // Capitalized words after the first textual word are treated as proper
        // names and are opaque to all word-level styling.
        bool seenWord = false;
        tokenBegin = 0;
        while (tokenBegin < text.size())
        {
            size_t tokenEnd = text.find_first_of(" \t\r\n", tokenBegin);
            if (tokenEnd == std::string::npos)
                tokenEnd = text.size();
            size_t wordBegin = tokenBegin;
            while (wordBegin < tokenEnd && !IsLetterByte(static_cast<unsigned char>(text[wordBegin])))
                ++wordBegin;
            if (wordBegin < tokenEnd)
            {
                size_t wordEnd = wordBegin;
                while (wordEnd < tokenEnd && IsLetterByte(static_cast<unsigned char>(text[wordEnd])))
                    ++wordEnd;
                if (seenWord && !IsProtectedRange(protectedBytes, tokenBegin, tokenEnd) &&
                    StartsUppercase(std::string_view(text).substr(wordBegin, wordEnd - wordBegin)))
                    protect(tokenBegin, tokenEnd);
                seenWord = true;
            }
            tokenBegin = tokenEnd + (tokenEnd < text.size() ? 1 : 0);
        }
        return protectedBytes;
    }

    std::vector<Word> FindWords(const std::string& text, const std::vector<bool>& protectedBytes)
    {
        std::vector<Word> words;
        size_t i = 0;
        while (i < text.size())
        {
            if (!IsLetterByte(static_cast<unsigned char>(text[i])))
            {
                ++i;
                continue;
            }
            const size_t begin = i;
            bool isProtected = false;
            while (i < text.size() && IsLetterByte(static_cast<unsigned char>(text[i])))
            {
                isProtected = isProtected || protectedBytes[i];
                ++i;
            }
            words.push_back({ begin, i, words.size(), isProtected });
        }
        return words;
    }

    bool IsProtectedRange(const std::vector<bool>& protectedBytes, size_t begin, size_t end)
    {
        for (size_t i = begin; i < end && i < protectedBytes.size(); ++i)
            if (protectedBytes[i])
                return true;
        return false;
    }

    std::string ApplyAbbreviations(const std::string& text,
                                   const std::vector<Abbreviation>& abbreviations)
    {
        if (abbreviations.empty())
            return text;
        const std::vector<bool> protectedBytes = FindProtectedBytes(text);
        const std::vector<Word> words = FindWords(text, protectedBytes);
        std::string result;
        result.reserve(text.size());
        size_t cursor = 0;
        size_t wordIndex = 0;

        while (cursor < text.size())
        {
            while (wordIndex < words.size() && words[wordIndex].begin < cursor)
                ++wordIndex;
            if (wordIndex >= words.size() || words[wordIndex].begin != cursor ||
                words[wordIndex].protectedToken ||
                StartsUppercase(std::string_view(text).substr(words[wordIndex].begin,
                                                               words[wordIndex].end - words[wordIndex].begin)))
            {
                result.push_back(text[cursor++]);
                continue;
            }

            const Abbreviation* matched = nullptr;
            size_t matchedEnd = cursor;
            for (const Abbreviation& abbreviation : abbreviations)
            {
                if (abbreviation.from.empty() || cursor + abbreviation.from.size() > text.size())
                    continue;
                const size_t end = cursor + abbreviation.from.size();
                if (IsProtectedRange(protectedBytes, cursor, end))
                    continue;
                if (LowerUtf8(std::string_view(text).substr(cursor, abbreviation.from.size())) != abbreviation.from)
                    continue;
                if (cursor > 0 && IsWordByte(static_cast<unsigned char>(text[cursor - 1])))
                    continue;
                if (end < text.size() && IsWordByte(static_cast<unsigned char>(text[end])))
                    continue;
                matched = &abbreviation;
                matchedEnd = end;
                break;
            }

            if (matched)
            {
                result += matched->to;
                cursor = matchedEnd;
            }
            else
            {
                result.push_back(text[cursor++]);
            }
        }
        return result;
    }

    std::string LowercaseMessageStart(const std::string& text)
    {
        const std::vector<bool> protectedBytes = FindProtectedBytes(text);
        const std::vector<Word> words = FindWords(text, protectedBytes);
        for (const Word& word : words)
        {
            if (word.protectedToken)
                continue;
            std::string result = text;
            result.replace(word.begin, word.end - word.begin,
                           LowerFirstLetter(std::string_view(text).substr(word.begin, word.end - word.begin)));
            return result;
        }
        return text;
    }

    std::string RemoveAccentsFromWords(const std::string& text)
    {
        const std::vector<bool> protectedBytes = FindProtectedBytes(text);
        std::string result;
        result.reserve(text.size());
        for (size_t i = 0; i < text.size();)
        {
            if (protectedBytes[i])
            {
                result.push_back(text[i++]);
                continue;
            }
            if (IsLetterByte(static_cast<unsigned char>(text[i])))
            {
                size_t end = i + 1;
                while (end < text.size() && IsLetterByte(static_cast<unsigned char>(text[end])) && !protectedBytes[end])
                    ++end;
                result += RemoveAccents(std::string_view(text).substr(i, end - i));
                i = end;
                continue;
            }
            result.push_back(text[i++]);
        }
        return result;
    }

    std::string RemoveOpeningMarks(const std::string& text)
    {
        const std::vector<bool> protectedBytes = FindProtectedBytes(text);
        std::string result;
        result.reserve(text.size());
        for (size_t i = 0; i < text.size();)
        {
            if (!protectedBytes[i] && i + 1 < text.size() &&
                static_cast<unsigned char>(text[i]) == 0xC2 &&
                (static_cast<unsigned char>(text[i + 1]) == 0xA1 ||
                 static_cast<unsigned char>(text[i + 1]) == 0xBF) &&
                !protectedBytes[i + 1])
            {
                i += 2;
                continue;
            }
            result.push_back(text[i++]);
        }
        return result;
    }

    std::string RemoveFinalPeriod(const std::string& text)
    {
        size_t end = text.size();
        while (end > 0 && (text[end - 1] == ' ' || text[end - 1] == '\t' ||
                           text[end - 1] == '\r' || text[end - 1] == '\n'))
            --end;
        const std::vector<bool> protectedBytes = FindProtectedBytes(text);
        if (end > 0 && text[end - 1] == '.' && !protectedBytes[end - 1])
        {
            std::string result = text;
            result.erase(end - 1, 1);
            return result;
        }
        return text;
    }

    std::string ApplyOneTypo(const std::string& text, uint32_t maxPercent, uint64_t botGuid)
    {
        if (maxPercent == 0)
            return text;

        const uint32_t typoRate = static_cast<uint32_t>(FeatureHash(botGuid, "typo-rate") % (maxPercent + 1));
        if (typoRate == 0)
            return text;

        const std::vector<bool> protectedBytes = FindProtectedBytes(text);
        const std::vector<Word> words = FindWords(text, protectedBytes);
        std::string result = text;
        bool typoUsed = false;
        for (const Word& word : words)
        {
            if (typoUsed || word.protectedToken || word.end - word.begin < 4 ||
                (word.ordinal > 0 && StartsUppercase(std::string_view(text).substr(word.begin, word.end - word.begin))))
                continue;

            bool lowercaseAscii = true;
            for (size_t i = word.begin; i < word.end; ++i)
                if (text[i] < 'a' || text[i] > 'z')
                {
                    lowercaseAscii = false;
                    break;
                }
            if (!lowercaseAscii || RandomInclusive(1, 100) > typoRate)
                continue;

            const uint32_t operation = RandomInclusive(0, 2);
            size_t index = RandomInclusive(0, static_cast<uint32_t>(word.end - word.begin - 1));
            if (operation == 0 && word.end - word.begin >= 2)
            {
                if (index + 1 >= word.end - word.begin)
                    --index;
                std::swap(result[word.begin + index], result[word.begin + index + 1]);
            }
            else if (operation == 1)
            {
                result.insert(word.begin + index, 1, result[word.begin + index]);
            }
            else
            {
                result.erase(word.begin + index, 1);
            }
            typoUsed = true;
        }
        return result;
    }
}

void Style_Configure(bool enabled,
                     uint32_t lowercaseStartChance,
                     uint32_t dropFinalPeriodChance,
                     uint32_t dropOpeningMarksChance,
                     uint32_t noAccentsChance,
                     uint32_t abbreviationsChance,
                     uint32_t typoPercent,
                     const std::string& abbreviations)
{
    StyleSettings settings;
    settings.enabled = enabled;
    settings.lowercaseStartChance = ClampChance(lowercaseStartChance);
    settings.dropFinalPeriodChance = ClampChance(dropFinalPeriodChance);
    settings.dropOpeningMarksChance = ClampChance(dropOpeningMarksChance);
    settings.noAccentsChance = ClampChance(noAccentsChance);
    settings.abbreviationsChance = ClampChance(abbreviationsChance);
    settings.typoPercent = ClampChance(typoPercent);

    size_t begin = 0;
    while (begin <= abbreviations.size())
    {
        size_t end = abbreviations.find('|', begin);
        if (end == std::string::npos)
            end = abbreviations.size();
        const std::string entry = Trim(abbreviations.substr(begin, end - begin));
        const size_t equals = entry.find('=');
        if (equals != std::string::npos)
        {
            std::string from = LowerUtf8(Trim(entry.substr(0, equals)));
            std::string to = Trim(entry.substr(equals + 1));
            if (!from.empty() && !to.empty())
                settings.abbreviations.push_back({ std::move(from), std::move(to) });
        }
        if (end == abbreviations.size())
            break;
        begin = end + 1;
    }

    std::lock_guard<std::mutex> lock(g_styleMutex);
    g_styleSettings = std::move(settings);
}

std::string Style_Apply(const std::string& text, uint64_t botGuid)
{
    if (text.empty())
        return text;

    StyleSettings settings;
    {
        std::lock_guard<std::mutex> lock(g_styleMutex);
        settings = g_styleSettings;
    }
    if (!settings.enabled)
        return text;

    const bool lowercaseStart = HasTrait(botGuid, "lowercase-start", settings.lowercaseStartChance) && ApplyThisMessage();
    const bool dropFinalPeriod = HasTrait(botGuid, "drop-final-period", settings.dropFinalPeriodChance) && ApplyThisMessage();
    const bool dropOpeningMarks = HasTrait(botGuid, "drop-opening-marks", settings.dropOpeningMarksChance) && ApplyThisMessage();
    const bool noAccents = HasTrait(botGuid, "no-accents", settings.noAccentsChance) && ApplyThisMessage();
    const bool abbreviate = HasTrait(botGuid, "abbreviations", settings.abbreviationsChance) && ApplyThisMessage();
    const bool typo = settings.typoPercent > 0 && ApplyThisMessage();

    std::string result = text;
    if (dropOpeningMarks)
        result = RemoveOpeningMarks(result);
    if (abbreviate)
        result = ApplyAbbreviations(result, settings.abbreviations);
    if (noAccents)
        result = RemoveAccentsFromWords(result);
    if (lowercaseStart)
        result = LowercaseMessageStart(result);
    if (typo)
        result = ApplyOneTypo(result, settings.typoPercent, botGuid);
    if (dropFinalPeriod)
        result = RemoveFinalPeriod(result);

    return result.empty() ? text : result;
}
