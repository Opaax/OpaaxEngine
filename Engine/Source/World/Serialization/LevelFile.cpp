#include "World/Serialization/LevelFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"

namespace Opaax
{
    bool LevelFile::Load(const OpaaxString& InAbsPath, LevelData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogLevelFile, Error, "Level '{}' is missing, empty or unreadable", InAbsPath.CStr())
            return false;
        }

        // No exceptions: a hand-edited manifest is an ordinary input, not an exceptional one.
        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);
        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogLevelFile, Error, "Level '{}' is not a json object", InAbsPath.CStr())
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
                      InAbsPath.CStr(), lVersion, LEVEL_FORMAT_VERSION)
            return false;
        }

        // Built into a LOCAL and moved out at the end, so every failure above leaves the
        // caller's level exactly as it was (MapFile::Load holds the same contract).
        LevelData lParsed;

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

        OPAAX_LOG(LogLevelFile, Info, "Loaded level '{}': {} map(s)", InAbsPath.CStr(), lParsed.MapCount())

        OutData = Move(lParsed);
        return true;
    }
}
