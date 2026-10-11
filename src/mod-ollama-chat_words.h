#ifndef MOD_OLLAMA_CHAT_WORDS_H
#define MOD_OLLAMA_CHAT_WORDS_H

#include "Common.h"
#include "SharedDefines.h"

#include <string>
#include <utility>
#include <vector>

class Guild;
class PlayerbotAI;

void Words_Configure(const std::string& male, const std::string& female,
                     const std::string& alliance, const std::string& horde,
                     const std::string& inGroup, const std::string& solo,
                     const std::string& noGuild, const std::string& thePlayer,
                     const std::string& promptLocale);
void Words_ConfigureReplyLengthBuckets(const std::string& buckets);
uint32_t Words_SampleMaxWords();

std::string Words_Gender(uint8 gender);
std::string Words_Faction(TeamId faction);
std::string Words_GroupStatus(bool inGroup);
std::string Words_GuildOrNone(Guild* guild);
std::string Words_ThePlayer();
std::string Words_ClassName(uint8 classId, PlayerbotAI* ai = nullptr, const char* fallback = nullptr);
std::string Words_RaceName(uint8 raceId, PlayerbotAI* ai = nullptr, const char* fallback = nullptr);
std::string Words_SnapshotClassName(uint8 classId);
std::string Words_SnapshotRaceName(uint8 raceId);

#endif
