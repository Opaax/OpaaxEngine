#include "World/Serialization/LevelFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxPathString.h"

namespace Opaax
{
    bool LevelFile::Load(const OpaaxString& InAbsPath, LevelData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogLevelFile, Error, "Level '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        // No exceptions: a malformed file is a normal input.
        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);
        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogLevelFile, Error, "Level '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        const auto   lVersionIt = lJson.find(KEY_VERSION);
        const Uint32 lVersion   = (lVersionIt != lJson.end() && lVersionIt->is_number_unsigned())
                                      ? lVersionIt->get<Uint32>()
                                      : 0u;

        if (lVersion > LEVEL_FORMAT_VERSION)
        {
            OPAAX_LOG(LogLevelFile, Error,
                      "Level '{}' is format version {}, newer than this build reads ({}) — refusing",
                      InAbsPath.CStr(), lVersion, LEVEL_FORMAT_VERSION);
            return false;
        }

        // Built in a local and moved at the end, so OutData is untouched on failure.
        LevelData lParsed;

        const auto lNameIt = lJson.find(KEY_NAME);
        if (lNameIt != lJson.end() && lNameIt->is_string())
        {
            lParsed.Name = OpaaxString(lNameIt->get<std::string>().c_str());
        }

        // Stem only as a fallback: a level with a name keeps it when moved.
        if (lParsed.Name.IsEmpty())
        {
            lParsed.Name = PathString::Stem(InAbsPath).ToString();
        }

        const auto lMapsIt = lJson.find(KEY_MAPS);
        if (lMapsIt != lJson.end() && lMapsIt->is_array())
        {
            for (const nlohmann::json& lEntry : *lMapsIt)
            {
                // Skip a non-string entry instead of failing the whole level.
                if (!lEntry.is_string()) { continue; }

                const std::string lPath = lEntry.get<std::string>();
                if (lPath.empty()) { continue; }

                lParsed.Maps.emplace_back(lPath.c_str());
            }
        }

        // Saved as a path, held as an index (checked once, here).
        const auto lPersistIt = lJson.find(KEY_PERSISTENT_MAP);
        if (lPersistIt != lJson.end() && lPersistIt->is_string())
        {
            const OpaaxString lWanted(lPersistIt->get<std::string>().c_str());

            for (Uint64 lIndex = 0; lIndex < lParsed.MapCount(); ++lIndex)
            {
                if (lParsed.Maps[lIndex] == lWanted) { lParsed.PersistentMapIndex = lIndex; break; }
            }

            // Warn: the level still loads, with its first map persistent.
            if (!lWanted.IsEmpty() && lParsed.PersistentMap() != lWanted)
            {
                OPAAX_LOG(LogLevelFile, Warn,
                          "Level '{}' names persistent map '{}', which is not one of its 'maps' — "
                          "the first map is persistent instead", InAbsPath.CStr(), lWanted.CStr());
            }
        }

        OPAAX_LOG(LogLevelFile, Trace, "Loaded level '{}' as '{}': {} map(s), persistent '{}'",
                  InAbsPath.CStr(), lParsed.Name.CStr(), lParsed.MapCount(),
                  lParsed.IsEmpty() ? "(none)" : lParsed.PersistentMap().CStr());

        OutData = Move(lParsed);
        return true;
    }

    OpaaxString LevelFile::Serialize(const LevelData& InData)
    {
        nlohmann::json lJson;

        lJson[KEY_VERSION] = LEVEL_FORMAT_VERSION;
        lJson[KEY_NAME]    = std::string(InData.Name.CStr());

        nlohmann::json lMaps = nlohmann::json::array();
        for (const OpaaxString& lMap : InData.Maps)
        {
            lMaps.emplace_back(lMap.CStr());
        }
        lJson[KEY_MAPS] = Move(lMaps);

        // Always written, so reordering the list keeps the persistent map.
        if (!InData.IsEmpty())
        {
            lJson[KEY_PERSISTENT_MAP] = std::string(InData.PersistentMap().CStr());
        }

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool LevelFile::Save(const OpaaxString& InAbsPath, const LevelData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogLevelFile, Error, "Cannot write level '{}'", InAbsPath.CStr());
            return false;
        }

        // Log success too.
        OPAAX_LOG(LogLevelFile, Info, "Saved level '{}' — {} map(s), persistent '{}'",
                  InAbsPath.CStr(), InData.MapCount(),
                  InData.IsEmpty() ? "(none)" : InData.PersistentMap().CStr());

        return true;
    }
}
