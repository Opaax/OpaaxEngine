#include "World/Serialization/LevelFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"

namespace Opaax
{
    namespace
    {
        // Byte-wise on purpose: '/', '\\' and '.' are ASCII, so this is UTF-8 safe without
        // building an fs::path out of an engine string (I7).
        OpaaxString FileStem(const OpaaxString& InPath)
        {
            const std::string lPath(InPath.CStr());

            const size_t lSlash = lPath.find_last_of("/\\");
            const size_t lStart = (lSlash == std::string::npos) ? 0 : lSlash + 1;

            const size_t lDot = lPath.find_last_of('.');
            const size_t lEnd = (lDot == std::string::npos || lDot < lStart) ? lPath.size() : lDot;

            return OpaaxString(lPath.substr(lStart, lEnd - lStart).c_str());
        }
    }

    bool LevelFile::Load(const OpaaxString& InAbsPath, LevelData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogLevelFile, Error, "Level '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        // No exceptions: a hand-edited manifest is an ordinary input, not an exceptional one.
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

        // Built into a LOCAL and moved out at the end, so every failure above leaves the
        // caller's level exactly as it was (MapFile::Load holds the same contract).
        LevelData lParsed;

        const auto lNameIt = lJson.find(KEY_NAME);
        if (lNameIt != lJson.end() && lNameIt->is_string())
        {
            lParsed.Name = OpaaxString(lNameIt->get<std::string>().c_str());
        }

        // The stem is the fallback, never the source: a level that names itself keeps its name
        // wherever the file is moved to.
        if (lParsed.Name.IsEmpty())
        {
            lParsed.Name = FileStem(InAbsPath);
        }

        const auto lMapsIt = lJson.find(KEY_MAPS);
        if (lMapsIt != lJson.end() && lMapsIt->is_array())
        {
            for (const nlohmann::json& lEntry : *lMapsIt)
            {
                // A non-string entry is skipped rather than fatal: one bad line in a manifest
                // must not cost the author every other map in it.
                if (!lEntry.is_string()) { continue; }

                const std::string lPath = lEntry.get<std::string>();
                if (lPath.empty()) { continue; }

                lParsed.Maps.push_back(OpaaxString(lPath.c_str()));
            }
        }

        // Named by PATH in the file, held as an INDEX in memory — so "the persistent map is one of
        // this level's maps" (WM1a) is resolved once, here, and never re-checked downstream.
        const auto lPersistIt = lJson.find(KEY_PERSISTENT_MAP);
        if (lPersistIt != lJson.end() && lPersistIt->is_string())
        {
            const OpaaxString lWanted(lPersistIt->get<std::string>().c_str());

            for (Uint64 lIndex = 0; lIndex < lParsed.MapCount(); ++lIndex)
            {
                if (lParsed.Maps[lIndex] == lWanted) { lParsed.PersistentMapIndex = lIndex; break; }
            }

            // Loud when the name matched nothing. The level still loads with its first map
            // persistent (MP3) — and a silent default is exactly what makes that unspottable.
            if (!lWanted.IsEmpty() && lParsed.PersistentMap() != lWanted)
            {
                OPAAX_LOG(LogLevelFile, Warn,
                          "Level '{}' names persistent map '{}', which is not one of its 'maps' — "
                          "the first map is persistent instead", InAbsPath.CStr(), lWanted.CStr());
            }
        }

        OPAAX_LOG(LogLevelFile, Info, "Loaded level '{}' as '{}': {} map(s), persistent '{}'",
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
            lMaps.push_back(std::string(lMap.CStr()));
        }
        lJson[KEY_MAPS] = Move(lMaps);

        // Written even when it IS the first entry. "Absent means the first" is a READER's default
        // (MP3); a file that states its persistent map keeps it when someone reorders the list.
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

        // The SUCCESS branch is logged, not just the failures: "no error" and "it happened" are
        // different statements, and only this one discriminates ([[L15]]).
        OPAAX_LOG(LogLevelFile, Info, "Saved level '{}' — {} map(s), persistent '{}'",
                  InAbsPath.CStr(), InData.MapCount(),
                  InData.IsEmpty() ? "(none)" : InData.PersistentMap().CStr());

        return true;
    }
}
