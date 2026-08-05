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

        OPAAX_LOG(LogLevelFile, Info, "Loaded level '{}' as '{}': {} map(s)",
                  InAbsPath.CStr(), lParsed.Name.CStr(), lParsed.MapCount())

        OutData = Move(lParsed);
        return true;
    }
}
