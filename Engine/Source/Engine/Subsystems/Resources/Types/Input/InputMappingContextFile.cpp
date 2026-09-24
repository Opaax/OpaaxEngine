#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextData.h"

namespace Opaax
{
    OpaaxString InputMappingContextFile::Serialize(const InputMappingContextData& InData)
    {
        const nlohmann::json lJson = InData;

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool InputMappingContextFile::Save(const OpaaxString& InAbsPath, const InputMappingContextData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogInputMappingContextFile, Error, "Cannot write input map '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogInputMappingContextFile, Info, "Saved {} mapping(s) to '{}'",
                  InData.EntryCount(), InAbsPath.CStr());
        return true;
    }

    bool InputMappingContextFile::Load(const OpaaxString& InAbsPath, InputMappingContextData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogInputMappingContextFile, Error, "Input map '{}' is missing, empty or unreadable",
                      InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogInputMappingContextFile, Error, "Input map '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        try
        {
            OutData = lJson.get<InputMappingContextData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogInputMappingContextFile, Error, "Input map '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogInputMappingContextFile, Info, "Loaded {} mapping(s) at priority {} from '{}'",
                  OutData.EntryCount(), OutData.Priority, InAbsPath.CStr());
        return true;
    }
}
