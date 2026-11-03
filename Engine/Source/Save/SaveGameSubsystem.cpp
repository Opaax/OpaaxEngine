#include "Save/SaveGameSubsystem.h"

#include "Application/Services/ISaveStorage.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Resources/ResourceManager.h"
#include "Save/SaveGameFile.h"

namespace Opaax
{
    namespace
    {
        /** InNames as "A, B, C". */
        OpaaxString JoinNames(const TDynArray<OpaaxString>& InNames)
        {
            OpaaxString lJoined;
            for (const OpaaxString& lName : InNames)
            {
                if (!lJoined.IsEmpty()) { lJoined += ", "; }
                lJoined += lName;
            }
            return lJoined;
        }
    }

    // =========================================================================
    // Saving and loading
    // =========================================================================
    ESaveResult SaveGameSubsystem::Save(const OpaaxStringView InSlot, const SaveGame& InSave)
    {
        const ESaveResult lResult = GetContext().Saves.Write(InSlot, SaveGameFile::Serialize(InSave));
        ReportSave(InSlot, InSave.GetClass().GetName(), lResult);
        return lResult;
    }

    bool SaveGameSubsystem::Exists(const OpaaxStringView InSlot) const
    {
        return GetContext().Saves.Exists(InSlot);
    }

    ESaveResult SaveGameSubsystem::Delete(const OpaaxStringView InSlot)
    {
        const ESaveResult lResult = GetContext().Saves.Delete(InSlot);
        if (lResult == ESaveResult::Ok)
        {
            OPAAX_LOG(LogSaveGame, Info, "Deleted slot '{}'", InSlot);
        }
        return lResult;
    }

    TDynArray<OpaaxString> SaveGameSubsystem::ListSlots() const
    {
        return GetContext().Saves.ListSlots();
    }

    // =========================================================================
    // Internal
    // =========================================================================
    ESaveResult SaveGameSubsystem::LoadOfClass(const OpaaxStringView InSlot, const ObjectClass& InClass,
                                               TStrongObjectPtr<SaveGame>& OutSave)
    {
        const GameInstanceContext& lContext = GetContext();

        LoadOutcome lOutcome = ReadSlot(lContext.Saves, lContext.Resources.GetObjectClasses(), InSlot, InClass);
        ReportLoad(InSlot, lOutcome);

        if (lOutcome.Result == ESaveResult::Ok)
        {
            OutSave = Move(lOutcome.Save);
        }
        return lOutcome.Result;
    }

    SaveGameSubsystem::LoadOutcome SaveGameSubsystem::ReadSlot(const ISaveStorage& InStorage,
                                                               const ObjectClassRegistry* InClasses,
                                                               const OpaaxStringView InSlot, const ObjectClass& InClass)
    {
        LoadOutcome lOutcome;

        OpaaxString lText;
        lOutcome.Result = InStorage.Read(InSlot, lText);
        if (lOutcome.Result != ESaveResult::Ok)
        {
            return lOutcome;
        }

        SaveGameFile::Contents lContents;
        lOutcome.Result = SaveGameFile::Parse(lText, lContents, &lOutcome.Error);
        if (lOutcome.Result != ESaveResult::Ok)
        {
            return lOutcome;
        }

        if (InClasses == nullptr)
        {
            lOutcome.Result = ESaveResult::WrongClass;
            lOutcome.Error  = OpaaxString("the game has no class registry to make saves from");
            return lOutcome;
        }

        lOutcome.Result = SaveGameFile::Instantiate(lContents, *InClasses, InClass, OpaaxStringID(InSlot.ToString()),
                                                    lOutcome.Save, &lOutcome.Error, &lOutcome.BadFields);
        return lOutcome;
    }

    void SaveGameSubsystem::ReportLoad(const OpaaxStringView InSlot, const LoadOutcome& InOutcome)
    {
        switch (InOutcome.Result)
        {
            case ESaveResult::Ok:
                OPAAX_LOG(LogSaveGame, Info, "Loaded slot '{}' ({})", InSlot, InOutcome.Save->GetClass().GetName());
                if (!InOutcome.BadFields.empty())
                {
                    OPAAX_LOG(LogSaveGame, Warn, "Slot '{}': wrong value type for {}, kept at their defaults", InSlot,
                              JoinNames(InOutcome.BadFields).CStr());
                }
                break;

            case ESaveResult::Missing:
                OPAAX_LOG(LogSaveGame, Info, "Slot '{}' has no save yet", InSlot);
                break;

            case ESaveResult::InvalidSlot:
                OPAAX_LOG(LogSaveGame, Error, "'{}' is not a slot name: 1 to {} letters, digits, '_' or '-'", InSlot,
                          ISaveStorage::MAX_SLOT_NAME_LENGTH);
                break;

            // The storage said why.
            case ESaveResult::IoError:
                break;

            default:
                OPAAX_LOG(LogSaveGame, Error, "Slot '{}' did not load ({}): {}", InSlot, ToString(InOutcome.Result),
                          InOutcome.Error.CStr());
                break;
        }
    }

    void SaveGameSubsystem::ReportSave(const OpaaxStringView InSlot, const OpaaxStringID InClass, const ESaveResult InResult)
    {
        if (InResult == ESaveResult::Ok)
        {
            OPAAX_LOG(LogSaveGame, Info, "Saved slot '{}' ({})", InSlot, InClass);
        }
        else if (InResult == ESaveResult::InvalidSlot)
        {
            OPAAX_LOG(LogSaveGame, Error, "'{}' is not a slot name: 1 to {} letters, digits, '_' or '-'", InSlot,
                      ISaveStorage::MAX_SLOT_NAME_LENGTH);
        }
    }
}
