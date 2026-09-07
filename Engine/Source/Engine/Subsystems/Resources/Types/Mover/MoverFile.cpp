#include "Engine/Subsystems/Resources/Types/Mover/MoverFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverData.h"

namespace Opaax
{
    OpaaxString MoverFile::Serialize(const MoverData& InData)
    {
        const nlohmann::json lJson = InData;

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool MoverFile::Save(const OpaaxString& InAbsPath, const MoverData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogMoverFile, Error, "Cannot write mover '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogMoverFile, Info, "Saved {} mode name(s) to '{}'",
                  InData.EntryCount(), InAbsPath.CStr());
        return true;
    }

    bool MoverFile::Load(const OpaaxString& InAbsPath, MoverData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogMoverFile, Error, "Mover '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogMoverFile, Error, "Mover '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        try
        {
            OutData = lJson.get<MoverData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogMoverFile, Error, "Mover '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogMoverFile, Info, "Loaded {} mode name(s) from '{}'",
                  OutData.EntryCount(), InAbsPath.CStr());
        return true;
    }
}
