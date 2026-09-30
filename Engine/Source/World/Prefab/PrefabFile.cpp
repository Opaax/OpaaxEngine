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

        // Log success too.
        OPAAX_LOG(LogPrefabFile, Info, "Saved {} entity(ies) to '{}'", InData.EntityCount(),
                  InAbsPath.CStr());
        return true;
    }

    bool PrefabFile::Load(const OpaaxString& InAbsPath, PrefabData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // Missing or empty file: not a prefab.
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

        OPAAX_LOG(LogPrefabFile, Trace, "Loaded {} entity(ies) from '{}'", OutData.EntityCount(),
                  InAbsPath.CStr());
        return true;
    }
}
