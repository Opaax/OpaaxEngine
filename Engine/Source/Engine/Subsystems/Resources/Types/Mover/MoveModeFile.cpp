#include "Engine/Subsystems/Resources/Types/Mover/MoveModeFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"

namespace Opaax
{
    OpaaxString MoveModeFile::Serialize(const MoveModeData& InData)
    {
        const nlohmann::json lJson = InData;

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool MoveModeFile::Save(const OpaaxString& InAbsPath, const MoveModeData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogMoveModeFile, Error, "Cannot write move mode '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogMoveModeFile, Info, "Saved move mode '{}' to '{}'",
                  InData.Mode.ToString().CStr(), InAbsPath.CStr());
        return true;
    }

    bool MoveModeFile::Load(const OpaaxString& InAbsPath, MoveModeData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogMoveModeFile, Error, "Move mode '{}' is missing, empty or unreadable",
                      InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogMoveModeFile, Error, "Move mode '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        try
        {
            OutData = lJson.get<MoveModeData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogMoveModeFile, Error, "Move mode '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogMoveModeFile, Info, "Loaded move mode '{}' from '{}'",
                  OutData.Mode.ToString().CStr(), InAbsPath.CStr());
        return true;
    }
}
