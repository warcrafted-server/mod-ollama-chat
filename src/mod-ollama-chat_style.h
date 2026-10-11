#ifndef MOD_OLLAMA_CHAT_STYLE_H
#define MOD_OLLAMA_CHAT_STYLE_H

#include <cstdint>
#include <string>

void Style_Configure(bool enabled,
                     uint32_t lowercaseStartChance,
                     uint32_t dropFinalPeriodChance,
                     uint32_t dropOpeningMarksChance,
                     uint32_t noAccentsChance,
                     uint32_t abbreviationsChance,
                     uint32_t typoPercent,
                     const std::string& abbreviations);

std::string Style_Apply(const std::string& text, uint64_t botGuid);

#endif
