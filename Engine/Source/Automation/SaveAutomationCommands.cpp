#include "Automation/SaveAutomationCommands.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ISaveStorage.h"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Object/ObjectClassRegistry.h"
#include "Save/SaveGame.h"
#include "Save/SaveGameFile.h"
#include "Save/SaveGameSubsystem.h"

namespace Opaax::SaveAutomation
{
    namespace
    {
        /** The slot InParams names, or empty with OutError saying what to give. */
        OpaaxString SlotOf(const nlohmann::json& InParams, OpaaxString& OutError)
        {
            const OpaaxString lSlot = InParams.value("slot", OpaaxString());
            if (!ISaveStorage::IsValidSlotName(lSlot))
            {
                OutError = lSlot.IsEmpty()
                               ? OpaaxString("give the {slot} (save.list lists them)")
                               : "'" + lSlot + "' is not a slot name: 1 to 64 letters, digits, '_' or '-'";
                return OpaaxString();
            }
            return lSlot;
        }

        /** InNames as "A, B, C". */
        OpaaxString JoinNames(const TDynArray<const char*>& InNames)
        {
            OpaaxString lJoined;
            for (const char* lName : InNames)
            {
                if (!lJoined.IsEmpty()) { lJoined += ", "; }
                lJoined += lName;
            }
            return lJoined;
        }

        /** Waits for the running game's background saves, so the files hold everything it asked for. */
        void FlushGameSaves(IEngine& InEngine)
        {
            if (GameInstance* const lGame = InEngine.GetGameInstances().GetGameInstance())
            {
                if (SaveGameSubsystem* const lSaves = lGame->GetSubsystems().GetSubsystem<SaveGameSubsystem>())
                {
                    lSaves->Flush();
                }
            }
        }
    }

    void Register(ICommandRegistry& InCommands, IEngine& InEngine)
    {
        InCommands.Add("save.list", "The player's save slots: {folder, slots}.",
            [&InEngine](const nlohmann::json&)
            {
                FlushGameSaves(InEngine);
                return List(OpaaxApplication::GetAppService<ISaveStorage>());
            });

        InCommands.Add("save.get",
            "A slot's save as its file holds it: {slot} answers {slot, path, class, classVersion, properties}.",
            [&InEngine](const nlohmann::json& InParams)
            {
                FlushGameSaves(InEngine);
                return Get(OpaaxApplication::GetAppService<ISaveStorage>(), InParams);
            });

        InCommands.Add("save.set",
            "Writes a slot: {slot, class, properties} as the game writes a save of that class (its defaults, "
            "properties merged), or {slot, text} written as given. Answers {slot, path}.",
            [&InEngine](const nlohmann::json& InParams)
            {
                FlushGameSaves(InEngine);
                return Set(OpaaxApplication::GetAppService<ISaveStorage>(), InEngine.GetRegistries().ObjectClasses(),
                           InParams);
            });

        InCommands.Add("save.clear", "Deletes a slot's save, {slot}, or every one, {all: true}. Answers {deleted}.",
            [&InEngine](const nlohmann::json& InParams)
            {
                FlushGameSaves(InEngine);
                return Clear(OpaaxApplication::GetAppService<ISaveStorage>(), InParams);
            });
    }

    AutomationResult List(const ISaveStorage& InStorage)
    {
        nlohmann::json lSlots = nlohmann::json::array();
        for (const OpaaxString& lSlot : InStorage.ListSlots())
        {
            lSlots.push_back(lSlot);
        }

        return AutomationResult::Ok(nlohmann::json{ { "folder", InStorage.GetSaveDirectory() }, { "slots", lSlots } });
    }

    AutomationResult Get(const ISaveStorage& InStorage, const nlohmann::json& InParams)
    {
        OpaaxString       lError;
        const OpaaxString lSlot = SlotOf(InParams, lError);
        if (lSlot.IsEmpty())
        {
            return AutomationResult::Fail(lError);
        }

        const OpaaxString lPath = InStorage.GetSlotPath(lSlot);

        OpaaxString       lText;
        const ESaveResult lRead = InStorage.Read(lSlot, lText);
        if (lRead == ESaveResult::Missing)
        {
            return AutomationResult::Fail("slot '" + lSlot + "' has no save (save.list lists the slots)");
        }
        if (lRead != ESaveResult::Ok)
        {
            return AutomationResult::Fail("slot '" + lSlot + "' could not be read ('" + lPath + "')");
        }

        SaveGameFile::Contents lContents;
        if (SaveGameFile::Parse(lText, lContents, &lError) != ESaveResult::Ok)
        {
            return AutomationResult::Fail("slot '" + lSlot + "' does not hold a save this engine reads: " + lError
                                          + " ('" + lPath + "')");
        }

        return AutomationResult::Ok(nlohmann::json{
            { "slot",         lSlot },
            { "path",         lPath },
            { "class",        lContents.Class.CStr() },
            { "classVersion", lContents.ClassVersion },
            { "properties",   lContents.Properties } });
    }

    AutomationResult Set(ISaveStorage& InStorage, const ObjectClassRegistry& InClasses, const nlohmann::json& InParams)
    {
        OpaaxString       lError;
        const OpaaxString lSlot = SlotOf(InParams, lError);
        if (lSlot.IsEmpty())
        {
            return AutomationResult::Fail(lError);
        }

        OpaaxString lText;

        // Written as given: how a test hands the game an old or broken file.
        if (const auto lRaw = InParams.find("text"); lRaw != InParams.end())
        {
            if (!lRaw->is_string())
            {
                return AutomationResult::Fail("{text} is the file's text, a string");
            }
            lText = lRaw->get<OpaaxString>();
        }
        else
        {
            const OpaaxString  lClassName = InParams.value("class", OpaaxString());
            const ObjectClass* lClass     = lClassName.IsEmpty() ? nullptr : InClasses.FindByName(OpaaxStringID(lClassName));
            if (lClass == nullptr || lClass->IsAbstract() || !lClass->IsChildOf(SaveGame::StaticClass()))
            {
                return AutomationResult::Fail("give the {class} of the save, a SaveGame class the game registers, "
                                              "or the file's {text}");
            }

            const nlohmann::json lProperties = InParams.value("properties", nlohmann::json::object());
            if (!lProperties.is_object())
            {
                return AutomationResult::Fail("{properties} is an object: {\"Name\": value}");
            }

            // A misspelt name would be dropped without a word: refused instead.
            const TDynArray<const char*> lNames = lClass->GetPropertyNames();
            for (const auto& [lKey, lValue] : lProperties.items())
            {
                bool bKnown = false;
                for (const char* lName : lNames)
                {
                    bKnown = bKnown || lKey == lName;
                }

                if (!bKnown)
                {
                    return AutomationResult::Fail("'" + lClassName + "' has no property '" + FromJsonString(lKey)
                                                  + "' (it has: " + JoinNames(lNames) + ")");
                }
            }

            const TStrongObjectPtr<SaveGame> lSave = Cast<SaveGame>(lClass->CreateInstance(OpaaxStringID(lSlot)));

            TDynArray<OpaaxString> lBadFields;
            lClass->ReadProperties(*lSave, lProperties, &lBadFields);
            if (!lBadFields.empty())
            {
                OpaaxString lFields;
                for (const OpaaxString& lField : lBadFields)
                {
                    if (!lFields.IsEmpty()) { lFields += ", "; }
                    lFields += lField;
                }
                return AutomationResult::Fail("wrong value type for " + lFields + ": nothing written");
            }

            lText = SaveGameFile::Serialize(*lSave);
        }

        const ESaveResult lWritten = InStorage.Write(lSlot, lText);
        if (lWritten != ESaveResult::Ok)
        {
            return AutomationResult::Fail("slot '" + lSlot + "' could not be written (" + ToString(lWritten) + ")");
        }

        return AutomationResult::Ok(nlohmann::json{ { "slot", lSlot }, { "path", InStorage.GetSlotPath(lSlot) } });
    }

    AutomationResult Clear(ISaveStorage& InStorage, const nlohmann::json& InParams)
    {
        TDynArray<OpaaxString> lSlots;

        if (InParams.value("all", false))
        {
            lSlots = InStorage.ListSlots();
        }
        else
        {
            OpaaxString       lError;
            const OpaaxString lSlot = SlotOf(InParams, lError);
            if (lSlot.IsEmpty())
            {
                return AutomationResult::Fail("give the {slot} to delete, or {all: true}");
            }
            lSlots.push_back(lSlot);
        }

        nlohmann::json lDeleted = nlohmann::json::array();
        for (const OpaaxString& lSlot : lSlots)
        {
            const ESaveResult lResult = InStorage.Delete(lSlot);
            if (lResult == ESaveResult::Ok)
            {
                lDeleted.push_back(lSlot);
            }
            else if (lResult != ESaveResult::Missing)
            {
                return AutomationResult::Fail("slot '" + lSlot + "' could not be deleted (" + ToString(lResult) + ")");
            }
        }

        return AutomationResult::Ok(nlohmann::json{ { "deleted", lDeleted } });
    }
}
