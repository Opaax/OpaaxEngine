#include "World/Serialization/MapFile.h"

#include <string>

#include "Core/IO/FileIO.h"
#include "World/Serialization/MapJson.h"

namespace Opaax
{
    // The only layer that can answer this — MapJson has the text, MapFile has the path.
    MapId MapFile::StemId(const OpaaxString& InAbsPath)
    {
        const std::string lPath  = std::string(InAbsPath.CStr());
        const size_t      lSlash = lPath.find_last_of("/\\");
        const size_t      lStart = (lSlash == std::string::npos) ? 0 : lSlash + 1;
        const size_t      lDot   = lPath.find_last_of('.');
        const size_t      lEnd   = (lDot == std::string::npos || lDot < lStart) ? lPath.size() : lDot;
        const std::string lStem  = lPath.substr(lStart, lEnd - lStart);

        return lStem.empty() ? MapId() : MapId(lStem);
    }

    bool MapFile::Save(const OpaaxString& InAbsPath, const MapData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, MapJson::Serialize(InData)))
        {
            OPAAX_LOG(LogMapFile, Error, "Cannot write map '{}'", InAbsPath.CStr());
            return false;
        }

        // The SUCCESS branch is logged, not just the failures: "no error" and "it happened" are
        // different statements, and only this one discriminates ([[L15]]).
        OPAAX_LOG(LogMapFile, Info, "Saved {} entity(ies) to '{}'", InData.EntityCount(), InAbsPath.CStr());
        return true;
    }

    bool MapFile::Load(const OpaaxString& InAbsPath, MapData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // FileIO is tolerant by contract and answers "" for both a missing file and an empty
        // one. Telling them apart would mean reaching IFileSystem, which lives behind the
        // service locator and is not reachable from the World layer — and it would change
        // nothing here, because an empty file is not a map either way.
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogMapFile, Error, "Map '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        // Deserialize leaves OutData untouched on every failure path, so a map that fails to
        // parse cannot half-replace the one the caller is holding.
        if (!MapJson::Deserialize(lText, OutData))
        {
            OPAAX_LOG(LogMapFile, Error, "Map '{}' could not be read (see the MapJson error above)",
                      InAbsPath.CStr());
            return false;
        }

        // LAST FALLBACK, and it is what makes "every map that loads has an identity" TOTAL
        // (**MP10**). MapJson already tried the `mapId` key and then the entities; a file with
        // neither — an empty map written before the key existed — would otherwise mount as
        // anonymous, and an invalid MapId is read as "the whole world" one layer up.
        if (!OutData.Id.IsValid())
        {
            OutData.Id = MapFile::StemId(InAbsPath);
        }

        OPAAX_LOG(LogMapFile, Info, "Loaded {} entity(ies) from '{}' (map '{}')",
                  OutData.EntityCount(), InAbsPath.CStr(),
                  OutData.Id.IsValid() ? OutData.Id.ToString().CStr() : "(none)");
        return true;
    }
}
