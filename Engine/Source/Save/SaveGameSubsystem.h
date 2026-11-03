#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "Object/ObjectPtr.h"
#include "Save/SaveGame.h"
#include "Save/SaveResult.h"

namespace Opaax
{
    class ISaveStorage;
    class ObjectClassRegistry;

    inline constexpr LogCategory LogSaveGame{"SaveGame"};

    // =============================================================================
    // SaveGameSubsystem — a game's saves: SaveGame objects to and from the player's slots
    //   (GetContext().Saves). Every game has one; a behaviour finds it with
    //   GetGameSubsystem<Opaax::SaveGameSubsystem>():
    //
    //   Opaax::TStrongObjectPtr<BhopSave> lSave;
    //   if (lSaves->Load("Main", lSave) == Opaax::ESaveResult::Missing)
    //   {
    //       lSave = Opaax::NewObject<BhopSave>();     // a first launch
    //   }
    //   lSaves->Save("Main", *lSave);
    //
    //   Only Missing means "start a new save": a slot that is Corrupt or from a NewerVersion still holds
    //   the player's progress, and saving over it loses it.
    // =============================================================================
    class SaveGameSubsystem final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(SaveGameSubsystem, GameInstanceSubsystem)

        // =========================================================================
        // Saving and loading
        // =========================================================================
    public:
        /**
         * Reads InSlot as a T (its class or a child of it).
         * @param OutSave Receives the save, named after the slot. Untouched on failure.
         * @return Missing when the slot has no save; Corrupt, NewerVersion, WrongClass, InvalidSlot or
         *   IoError (logged)
         */
        template<typename T>
        requires CDerivedFrom<T, SaveGame>
        ESaveResult Load(OpaaxStringView InSlot, TStrongObjectPtr<T>& OutSave)
        {
            TStrongObjectPtr<SaveGame> lSave;
            const ESaveResult          lResult = LoadOfClass(InSlot, T::StaticClass(), lSave);
            if (lResult == ESaveResult::Ok)
            {
                OutSave = Cast<T>(lSave);
            }
            return lResult;
        }

        /**
         * Writes InSave as InSlot's file, whole or not at all.
         * @return InvalidSlot or IoError (logged): the slot keeps its last save
         */
        ESaveResult Save(OpaaxStringView InSlot, const SaveGame& InSave);

        /** True when InSlot has a save. */
        bool Exists(OpaaxStringView InSlot) const;

        /**
         * Deletes InSlot's save.
         * @return Missing when it had none
         */
        ESaveResult Delete(OpaaxStringView InSlot);

        /** The slots that have a save, sorted by name. */
        TDynArray<OpaaxString> ListSlots() const;

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** What reading a slot gave. */
        struct LoadOutcome
        {
            ESaveResult                Result = ESaveResult::Missing;
            TStrongObjectPtr<SaveGame> Save;
            OpaaxString                Error;
            TDynArray<OpaaxString>     BadFields;
        };

        /** Load for a class known at run time. */
        ESaveResult LoadOfClass(OpaaxStringView InSlot, const ObjectClass& InClass, TStrongObjectPtr<SaveGame>& OutSave);

        /** Reads InSlot and makes its save. Touches nothing but its arguments: runs on any thread. */
        static LoadOutcome ReadSlot(const ISaveStorage& InStorage, const ObjectClassRegistry* InClasses,
                                    OpaaxStringView InSlot, const ObjectClass& InClass);

        /** Logs how a load ended. */
        static void ReportLoad(OpaaxStringView InSlot, const LoadOutcome& InOutcome);

        /** Logs how a save ended. */
        static void ReportSave(OpaaxStringView InSlot, OpaaxStringID InClass, ESaveResult InResult);
    };
}
