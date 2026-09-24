#include "Engine/Subsystems/Resources/Types/Input/InputActionFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/Input/InputActionData.h"

namespace Opaax
{
    OpaaxString InputActionFile::Serialize(const InputActionData& InData)
    {
        const nlohmann::json lJson = InData;

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool InputActionFile::Save(const OpaaxString& InAbsPath, const InputActionData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogInputActionFile, Error, "Cannot write input action '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogInputActionFile, Info, "Saved input action '{}' to '{}'",
                  InData.Name, InAbsPath.CStr());
        return true;
    }

    bool InputActionFile::Load(const OpaaxString& InAbsPath, InputActionData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogInputActionFile, Error, "Input action '{}' is missing, empty or unreadable",
                      InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogInputActionFile, Error, "Input action '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        try
        {
            OutData = lJson.get<InputActionData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogInputActionFile, Error, "Input action '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogInputActionFile, Info, "Loaded input action '{}' ({}) from '{}'",
                  OutData.Name, ToString(OutData.ValueType), InAbsPath.CStr());
        return true;
    }
}
