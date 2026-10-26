#include "Resources/Asset/AssetFile.h"

#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxStringJson.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetMigration.h"

namespace Opaax
{
    OpaaxString AssetFile::Serialize(const OpaaxStringID InClass, const nlohmann::json& InProperties)
    {
        const nlohmann::json lJson = {
            { "Class",      InClass.IsValid() ? InClass.CStr() : "" },
            { "Properties", InProperties.is_object() ? InProperties : nlohmann::json::object() },
            { "Version",    FORMAT_VERSION }
        };
        return ToJsonText(lJson, 4);
    }

    OpaaxString AssetFile::Serialize(const Asset& InAsset)
    {
        const ObjectClass& lClass = InAsset.GetClass();
        return Serialize(lClass.GetName(), lClass.WriteProperties(InAsset));
    }

    bool AssetFile::Save(const OpaaxString& InAbsPath, const Asset& InAsset)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InAsset)))
        {
            OPAAX_LOG(LogAssetFile, Error, "Cannot write asset '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogAssetFile, Info, "Saved asset '{}' ({})", InAbsPath.CStr(), InAsset.GetClass().GetName());
        return true;
    }

    bool AssetFile::Parse(const OpaaxString& InText, Contents& OutContents, OpaaxString* OutError)
    {
        const auto lFail = [OutError](const char* InWhy)
        {
            if (OutError != nullptr) { *OutError = OpaaxString(InWhy); }
            return false;
        };

        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            return lFail("not a json object");
        }

        const auto lClass      = lJson.find("Class");
        const auto lProperties = lJson.find("Properties");
        const auto lVersion    = lJson.find("Version");

        if (lClass == lJson.end() || !lClass->is_string() || lClass->get<OpaaxString>().IsEmpty())
        {
            return lFail("no \"Class\" name");
        }

        // A file written by hand may leave its properties out: they all keep their defaults.
        if (lProperties != lJson.end() && !lProperties->is_object())
        {
            return lFail("\"Properties\" is not an object");
        }

        Int32 lFileVersion = FORMAT_VERSION;
        if (lVersion != lJson.end())
        {
            if (!lVersion->is_number_integer())
            {
                return lFail("\"Version\" is not a whole number");
            }

            lFileVersion = lVersion->get<Int32>();
            if (lFileVersion > FORMAT_VERSION)
            {
                return lFail("written by a newer engine (\"Version\" above this engine's)");
            }
        }

        OutContents.Class      = OpaaxStringID(lClass->get<OpaaxString>());
        OutContents.Properties = (lProperties != lJson.end()) ? *lProperties : nlohmann::json::object();
        OutContents.Version    = lFileVersion;
        return true;
    }

    bool AssetFile::Load(const OpaaxString& InAbsPath, Contents& OutContents)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogAssetFile, Error, "Asset '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        OpaaxString lError;
        if (!Parse(lText, OutContents, &lError))
        {
            OPAAX_LOG(LogAssetFile, Error, "Asset '{}' is not an asset file: {}", InAbsPath.CStr(), lError.CStr());
            return false;
        }

        const OpaaxStringID lFileClass = OutContents.Class;
        if (AssetMigration::UpgradeSplitClass(OutContents))
        {
            OPAAX_LOG(LogAssetFile, Warn, "Asset '{}' is a {} of the old form: read as a {}. Start the editor once "
                      "with --upgrade-assets to rewrite it.", InAbsPath.CStr(), lFileClass, OutContents.Class);
        }

        return true;
    }

    OpaaxStringID AssetFile::FindClassName(const OpaaxString& InText)
    {
        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            return OpaaxStringID();
        }

        const auto lClass = lJson.find("Class");
        if (lClass == lJson.end() || !lClass->is_string())
        {
            return OpaaxStringID();
        }

        return OpaaxStringID::Find(lClass->get<OpaaxString>());
    }

    TStrongObjectPtr<Asset> AssetFile::Instantiate(const Contents& InContents, const ObjectClassRegistry& InClasses,
                                                   const OpaaxStringID InName, OpaaxString* OutError)
    {
        const auto lFail = [OutError](OpaaxString InWhy)
        {
            if (OutError != nullptr) { *OutError = Move(InWhy); }
            return TStrongObjectPtr<Asset>();
        };

        const ObjectClass* lClass = InClasses.FindByName(InContents.Class);

        if (lClass == nullptr)
        {
            return lFail(OpaaxString::Format("no class '{}' is registered (OPAAX_REGISTER_OBJECT)",
                                             InContents.Class.CStr()));
        }

        if (!lClass->IsChildOf(Asset::StaticClass()) || lClass->IsAbstract())
        {
            return lFail(OpaaxString::Format("class '{}' is not a concrete Asset", lClass->GetName().CStr()));
        }

        TStrongObjectPtr<Asset> lAsset = Cast<Asset>(lClass->CreateInstance(InName));

        // Strict like the rest of the engine's data: a value of the wrong type fails the load rather
        // than leaving a default the author did not write.
        TDynArray<OpaaxString> lBadFields;
        lClass->ReadProperties(*lAsset, InContents.Properties, &lBadFields);

        if (!lBadFields.empty())
        {
            OpaaxString lFields;
            for (const OpaaxString& lField : lBadFields)
            {
                if (!lFields.IsEmpty()) { lFields += ", "; }
                lFields += lField;
            }

            return lFail(OpaaxString::Format("wrong value type for {}", lFields.CStr()));
        }

        lAsset->PostLoad();
        return lAsset;
    }

    TStrongObjectPtr<Asset> AssetFile::LoadAsset(const OpaaxString& InAbsPath, const ObjectClassRegistry& InClasses)
    {
        Contents lContents;
        if (!Load(InAbsPath, lContents))
        {
            return {};
        }

        const OpaaxStringID lName(PathString::Stem(InAbsPath).ToString());

        OpaaxString lError;
        TStrongObjectPtr<Asset> lAsset = Instantiate(lContents, InClasses, lName, &lError);

        if (!lAsset.IsValid())
        {
            OPAAX_LOG(LogAssetFile, Error, "Asset '{}' did not load: {}", InAbsPath.CStr(), lError.CStr());
        }

        return lAsset;
    }
}
