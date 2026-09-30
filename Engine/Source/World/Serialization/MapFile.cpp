#include "World/Serialization/MapFile.h"

#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxPathString.h"
#include "World/Serialization/MapJson.h"

namespace Opaax
{
    // Only this layer has the path.
    MapId MapFile::StemId(const OpaaxString& InAbsPath)
    {
        const OpaaxStringView lStem = PathString::Stem(InAbsPath);

        return lStem.IsEmpty() ? MapId() : MapId(lStem.ToString());
    }

    bool MapFile::Save(const OpaaxString& InAbsPath, const MapData& InData)
    {
        return SaveText(InAbsPath, MapJson::Serialize(InData), InData.EntityCount());
    }

    bool MapFile::SaveText(const OpaaxString& InAbsPath, const OpaaxString& InText, Uint64 InEntityCount)
    {
        if (!FileIO::WriteAllText(InAbsPath, InText))
        {
            OPAAX_LOG(LogMapFile, Error, "Cannot write map '{}'", InAbsPath.CStr());
            return false;
        }

        // Log success too.
        OPAAX_LOG(LogMapFile, Info, "Saved {} entity(ies) to '{}'", InEntityCount, InAbsPath.CStr());
        return true;
    }

    bool MapFile::Load(const OpaaxString& InAbsPath, MapData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // Missing or empty file: not a map.
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogMapFile, Error, "Map '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        // OutData is untouched on failure.
        if (!MapJson::Deserialize(lText, OutData))
        {
            OPAAX_LOG(LogMapFile, Error, "Map '{}' could not be read (see the MapJson error above)",
                      InAbsPath.CStr());
            return false;
        }

        // Last fallback: name the map from its file, so every loaded map has an id.
        if (!OutData.Id.IsValid())
        {
            OutData.Id = MapFile::StemId(InAbsPath);
        }

        OPAAX_LOG(LogMapFile, Trace, "Loaded {} entity(ies) from '{}' (map '{}')",
                  OutData.EntityCount(), InAbsPath.CStr(),
                  OutData.Id.IsValid() ? OutData.Id.CStr() : "(none)");
        return true;
    }
}
