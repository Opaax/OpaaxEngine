#include "Resources/DataAsset/DataAssetFile.h"

#include "Core/IO/FileIO.h"

namespace Opaax
{
    OpaaxString DataAssetFile::Serialize(const OpaaxStringID InType, const nlohmann::json& InData)
    {
        const nlohmann::json lJson = {
            { "Type", InType.IsValid() ? InType.CStr() : "" },
            { "Data", InData.is_object() ? InData : nlohmann::json::object() }
        };
        return OpaaxString(lJson.dump(4).c_str());
    }

    bool DataAssetFile::Save(const OpaaxString& InAbsPath, const OpaaxStringID InType, const nlohmann::json& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InType, InData)))
        {
            OPAAX_LOG(LogDataAssetFile, Error, "Cannot write data asset '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogDataAssetFile, Info, "Saved data asset '{}' ({})", InAbsPath.CStr(),
                  InType.IsValid() ? InType.CStr() : "no type");
        return true;
    }

    bool DataAssetFile::Load(const OpaaxString& InAbsPath, Contents& OutContents)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogDataAssetFile, Error, "Data asset '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogDataAssetFile, Error, "Data asset '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        const auto lType = lJson.find("Type");
        const auto lData = lJson.find("Data");

        if (lType == lJson.end() || !lType->is_string() || lType->get_ref<const std::string&>().empty()
            || lData == lJson.end() || !lData->is_object())
        {
            OPAAX_LOG(LogDataAssetFile, Error, "Data asset '{}' needs a \"Type\" name and a \"Data\" object",
                      InAbsPath.CStr());
            return false;
        }

        OutContents.Type = OpaaxStringID(OpaaxString(lType->get_ref<const std::string&>().c_str()));
        OutContents.Data = *lData;
        return true;
    }
}
