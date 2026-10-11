#include "mod-ollama-chat_words.h"

#include "ChatHelper.h"
#include "DBCStores.h"
#include "Guild.h"
#include "PlayerbotAI.h"

namespace
{
    std::string g_Male = "Male";
    std::string g_Female = "Female";
    std::string g_Alliance = "Alliance";
    std::string g_Horde = "Horde";
    std::string g_InGroup = "In a group";
    std::string g_Solo = "Solo";
    std::string g_NoGuild = "No Guild";
    std::string g_ThePlayer = "The player";
    LocaleConstant g_PromptLocale = LOCALE_enUS;
    bool g_UsePromptLocale = false;

    std::string LocalizedName(char const* const* names)
    {
        char const* localized = names[g_PromptLocale];
        if (localized && localized[0])
            return localized;

        char const* english = names[LOCALE_enUS];
        return english ? english : "";
    }
}

void Words_Configure(const std::string& male, const std::string& female,
                     const std::string& alliance, const std::string& horde,
                     const std::string& inGroup, const std::string& solo,
                     const std::string& noGuild, const std::string& thePlayer,
                     const std::string& promptLocale)
{
    g_Male = male;
    g_Female = female;
    g_Alliance = alliance;
    g_Horde = horde;
    g_InGroup = inGroup;
    g_Solo = solo;
    g_NoGuild = noGuild;
    g_ThePlayer = thePlayer;

    g_UsePromptLocale = !promptLocale.empty();
    g_PromptLocale = g_UsePromptLocale ? GetLocaleByName(promptLocale) : LOCALE_enUS;
}

std::string Words_Gender(uint8 gender)
{
    return gender == GENDER_MALE ? g_Male : g_Female;
}

std::string Words_Faction(TeamId faction)
{
    return faction == TEAM_ALLIANCE ? g_Alliance : g_Horde;
}

std::string Words_GroupStatus(bool inGroup)
{
    return inGroup ? g_InGroup : g_Solo;
}

std::string Words_GuildOrNone(Guild* guild)
{
    return guild ? guild->GetName() : g_NoGuild;
}

std::string Words_ThePlayer()
{
    return g_ThePlayer;
}

std::string Words_ClassName(uint8 classId, PlayerbotAI* ai, const char* fallback)
{
    if (g_UsePromptLocale)
    {
        if (ChrClassesEntry const* entry = sChrClassesStore.LookupEntry(classId))
        {
            std::string name = LocalizedName(entry->name);
            if (!name.empty())
                return name;
        }
        return ChatHelper::FormatClass(classId);
    }

    if (ai && ai->GetChatHelper())
        return ai->GetChatHelper()->FormatClass(classId);
    return fallback ? fallback : ChatHelper::FormatClass(classId);
}

std::string Words_RaceName(uint8 raceId, PlayerbotAI* ai, const char* fallback)
{
    if (g_UsePromptLocale)
    {
        if (ChrRacesEntry const* entry = sChrRacesStore.LookupEntry(raceId))
        {
            std::string name = LocalizedName(entry->name);
            if (!name.empty())
                return name;
        }
        return ChatHelper::FormatRace(raceId);
    }

    if (ai && ai->GetChatHelper())
        return ai->GetChatHelper()->FormatRace(raceId);
    return fallback ? fallback : ChatHelper::FormatRace(raceId);
}

std::string Words_SnapshotClassName(uint8 classId)
{
    if (g_UsePromptLocale)
        return Words_ClassName(classId);

    switch (classId)
    {
        case CLASS_WARRIOR:      return "Warrior";
        case CLASS_PALADIN:      return "Paladin";
        case CLASS_HUNTER:       return "Hunter";
        case CLASS_ROGUE:        return "Rogue";
        case CLASS_PRIEST:       return "Priest";
        case CLASS_DEATH_KNIGHT: return "Death Knight";
        case CLASS_SHAMAN:       return "Shaman";
        case CLASS_MAGE:         return "Mage";
        case CLASS_WARLOCK:      return "Warlock";
        case CLASS_DRUID:        return "Druid";
        default:                 return "Unknown";
    }
}

std::string Words_SnapshotRaceName(uint8 raceId)
{
    if (g_UsePromptLocale)
        return Words_RaceName(raceId);

    switch (raceId)
    {
        case RACE_HUMAN:         return "Human";
        case RACE_ORC:           return "Orc";
        case RACE_DWARF:         return "Dwarf";
        case RACE_NIGHTELF:      return "Night Elf";
        case RACE_UNDEAD_PLAYER: return "Undead";
        case RACE_TAUREN:        return "Tauren";
        case RACE_GNOME:         return "Gnome";
        case RACE_TROLL:         return "Troll";
        case RACE_BLOODELF:      return "Blood Elf";
        case RACE_DRAENEI:       return "Draenei";
        default:                 return "Unknown";
    }
}
