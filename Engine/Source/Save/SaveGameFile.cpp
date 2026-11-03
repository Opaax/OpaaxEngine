#include "Save/SaveGameFile.h"

#include "Core/String/OpaaxStringJson.h"
#include "Object/ObjectClassRegistry.h"

namespace Opaax
{
    OpaaxString SaveGameFile::Serialize(const SaveGame& InSave)
    {
        const ObjectClass&   lClass      = InSave.GetClass();
        const nlohmann::json lProperties = lClass.WriteProperties(InSave);

        const nlohmann::json lJson = {
            { "Class",        lClass.GetName().CStr() },
            { "ClassVersion", InSave.GetSaveVersion() },
            { "Properties",   lProperties.is_object() ? lProperties : nlohmann::json::object() },
            { "Version",      FORMAT_VERSION }
        };
        return ToJsonText(lJson, 4);
    }

    ESaveResult SaveGameFile::Parse(const OpaaxString& InText, Contents& OutContents, OpaaxString* OutError)
    {
        const auto lFail = [OutError](const ESaveResult InResult, const char* InWhy)
        {
            if (OutError != nullptr) { *OutError = OpaaxString(InWhy); }
            return InResult;
        };

        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            return lFail(ESaveResult::Corrupt, "not a json object");
        }

        const auto lClass        = lJson.find("Class");
        const auto lProperties   = lJson.find("Properties");
        const auto lVersion      = lJson.find("Version");
        const auto lClassVersion = lJson.find("ClassVersion");

        if (lClass == lJson.end() || !lClass->is_string() || lClass->get<OpaaxString>().IsEmpty())
        {
            return lFail(ESaveResult::Corrupt, "no \"Class\" name");
        }

        if (lProperties != lJson.end() && !lProperties->is_object())
        {
            return lFail(ESaveResult::Corrupt, "\"Properties\" is not an object");
        }

        if (lVersion != lJson.end())
        {
            if (!lVersion->is_number_integer())
            {
                return lFail(ESaveResult::Corrupt, "\"Version\" is not a whole number");
            }

            if (lVersion->get<Int32>() > FORMAT_VERSION)
            {
                return lFail(ESaveResult::NewerVersion, "written by a newer engine (\"Version\" above this engine's)");
            }
        }

        Int32 lFileClassVersion = 1;
        if (lClassVersion != lJson.end())
        {
            if (!lClassVersion->is_number_integer())
            {
                return lFail(ESaveResult::Corrupt, "\"ClassVersion\" is not a whole number");
            }

            lFileClassVersion = lClassVersion->get<Int32>();
        }

        OutContents.Class        = OpaaxStringID(lClass->get<OpaaxString>());
        OutContents.Properties   = (lProperties != lJson.end()) ? *lProperties : nlohmann::json::object();
        OutContents.ClassVersion = lFileClassVersion;
        return ESaveResult::Ok;
    }

    ESaveResult SaveGameFile::Instantiate(const Contents& InContents, const ObjectClassRegistry& InClasses,
                                          const ObjectClass& InExpected, const OpaaxStringID InName,
                                          TStrongObjectPtr<SaveGame>& OutSave, OpaaxString* OutError,
                                          TDynArray<OpaaxString>* OutBadFields)
    {
        const auto lFail = [OutError](const ESaveResult InResult, OpaaxString InWhy)
        {
            if (OutError != nullptr) { *OutError = Move(InWhy); }
            return InResult;
        };

        const ObjectClass* lClass = InClasses.FindByName(InContents.Class);

        if (lClass == nullptr)
        {
            return lFail(ESaveResult::WrongClass,
                         OpaaxString::Format("no class '{}' is registered (OPAAX_REGISTER_OBJECT)",
                                             InContents.Class.CStr()));
        }

        if (!lClass->IsChildOf(SaveGame::StaticClass()) || !lClass->IsChildOf(InExpected) || lClass->IsAbstract())
        {
            return lFail(ESaveResult::WrongClass,
                         OpaaxString::Format("class '{}' is not a concrete {}", lClass->GetName().CStr(),
                                             InExpected.GetName().CStr()));
        }

        TStrongObjectPtr<SaveGame> lSave = Cast<SaveGame>(lClass->CreateInstance(InName));

        const Int32 lClassVersion = lSave->GetSaveVersion();
        if (InContents.ClassVersion > lClassVersion)
        {
            return lFail(ESaveResult::NewerVersion,
                         OpaaxString::Format("written by a newer game ({} version {}, this game reads up to {})",
                                             lClass->GetName().CStr(), InContents.ClassVersion, lClassVersion));
        }

        nlohmann::json lProperties = InContents.Properties;
        if (InContents.ClassVersion < lClassVersion)
        {
            lSave->UpgradeProperties(InContents.ClassVersion, lProperties);
        }

        if (!lClass->ReadProperties(*lSave, lProperties, OutBadFields))
        {
            return lFail(ESaveResult::Corrupt,
                         OpaaxString::Format("{}::UpgradeProperties left no properties object",
                                             lClass->GetName().CStr()));
        }

        lSave->PostLoad();
        OutSave = Move(lSave);
        return ESaveResult::Ok;
    }
}
