#include "World/Prefab/PrefabFile.h"

#include "Core/IO/FileIO.h"
#include "World/Prefab/PrefabJson.h"

namespace Opaax
{
    bool PrefabFile::Save(const OpaaxString& InAbsPath, const PrefabData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, PrefabJson::Serialize(InData)))
        {
            OPAAX_LOG(LogPrefabFile, Error, "Cannot write prefab '{}'", InAbsPath.CStr());
            return false;
        }

        // The SUCCESS branch is logged, not just the failures ([[L15]]).
        OPAAX_LOG(LogPrefabFile, Info, "Saved {} entity(ies) to '{}'", InData.EntityCount(),
                  InAbsPath.CStr());
        return true;
    }

    bool PrefabFile::Load(const OpaaxString& InAbsPath, PrefabData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // FileIO is tolerant by contract and answers "" for both a missing file and an empty one.
        // Telling them apart would mean reaching IFileSystem, which the World layer does not
        // reach — and it would change nothing, because an empty file is not a prefab either way.
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogPrefabFile, Error, "Prefab '{}' is missing, empty or unreadable",
                      InAbsPath.CStr());
            return false;
        }

        if (!PrefabJson::Deserialize(lText, OutData))
        {
            OPAAX_LOG(LogPrefabFile, Error,
                      "Prefab '{}' could not be read (see the PrefabJson error above)",
                      InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogPrefabFile, Info, "Loaded {} entity(ies) from '{}'", OutData.EntityCount(),
                  InAbsPath.CStr());
        return true;
    }
}
