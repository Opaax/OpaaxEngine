#pragma once

#include "Core/Jobs/JobHandle.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
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
    //   (GetContext().Saves), now or on a worker thread. Every game has one; a behaviour finds it with
    //   GetGameSubsystem<Opaax::SaveGameSubsystem>():
    //
    //   Opaax::TStrongObjectPtr<BhopSave> lSave;
    //   if (lSaves->Load("Main", lSave) == Opaax::ESaveResult::Missing)
    //   {
    //       lSave = Opaax::NewObject<BhopSave>();     // a first launch
    //   }
    //   lSaves->SaveAsync("Main", *lSave);          // written in the background
    //
    //   Only Missing means "start a new save": a slot that is Corrupt or from a NewerVersion still holds
    //   the player's progress, and saving over it loses it.
    //
    //   The background saves and loads run one at a time, in the order asked, so a load asked after a
    //   save reads it; a call that is not async waits for them first. Their callbacks run on the main
    //   thread, at the subsystem's next Update. When the game ends, TearDown waits for them and runs
    //   their callbacks while the game is still whole; what is asked later is still written at
    //   Shutdown, without its callback.
    // =============================================================================
    class SaveGameSubsystem final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(SaveGameSubsystem, GameInstanceSubsystem)

        // =========================================================================
        // Saving and loading
        // =========================================================================
    public:
        /**
         * Reads InSlot as a T (its class or a child of it). Waits for the background requests first.
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
         * Writes InSave as InSlot's file, whole or not at all. Waits for the background requests first.
         * @return InvalidSlot or IoError (logged): the slot keeps its last save
         */
        ESaveResult Save(OpaaxStringView InSlot, const SaveGame& InSave);

        /** True when InSlot has a save. Waits for the background requests first. */
        bool Exists(OpaaxStringView InSlot) const;

        /**
         * Deletes InSlot's save. Waits for the background requests first.
         * @return Missing when it had none
         */
        ESaveResult Delete(OpaaxStringView InSlot);

        /** The slots that have a save, sorted by name. Waits for the background requests first. */
        TDynArray<OpaaxString> ListSlots() const;

        // =========================================================================
        // In the background
        // =========================================================================
    public:
        /** Called with how a SaveAsync ended. */
        using FOnSaved = TFunction<void(ESaveResult)>;

        /**
         * Saves InSave to InSlot on a worker thread. InSave is turned into text now, so the game may change
         * it at once. InOnSaved runs on the main thread at the next Update, never inside this call.
         */
        void SaveAsync(OpaaxStringView InSlot, const SaveGame& InSave, FOnSaved InOnSaved = {});

        /**
         * Loads InSlot as a T on a worker thread. InOnLoaded gets how it ended and the save (null unless
         * Ok) on the main thread at the next Update, never inside this call.
         */
        template<typename T>
        requires CDerivedFrom<T, SaveGame>
        void LoadAsync(OpaaxStringView InSlot, TFunction<void(ESaveResult, TStrongObjectPtr<T>)> InOnLoaded)
        {
            LoadOfClassAsync(InSlot, T::StaticClass(),
                             [lOnLoaded = Move(InOnLoaded)](const ESaveResult InResult,
                                                            const TStrongObjectPtr<SaveGame>& InSave)
                             {
                                 if (lOnLoaded)
                                 {
                                     lOnLoaded(InResult, Cast<T>(InSave));
                                 }
                             });
        }

        /** Waits for every background save and load asked for, then runs their callbacks. Main thread. */
        void Flush();

        /** True from a SaveAsync or LoadAsync until its callback has run: a "saving..." icon. */
        bool IsBusy() const;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Subsystem interface
    public:
        bool Startup() override;
        void TearDown() override;
        void Shutdown() override;
        void Update(double InDeltaTime) override;
        //~End Subsystem interface

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** Called with how a load ended, for any save class. */
        using FOnLoadedSave = TFunction<void(ESaveResult, const TStrongObjectPtr<SaveGame>&)>;

        /** A save or a load, waiting for its turn. */
        struct Request
        {
            OpaaxString        Slot;
            OpaaxString        Text;                // a save's text
            OpaaxStringID      SaveClass;           // a save's class, for the log
            const ObjectClass* LoadClass = nullptr; // a load's class; null for a save
            FOnSaved           OnSaved;
            FOnLoadedSave      OnLoaded;
        };

        /** How a request ended. */
        struct Outcome
        {
            ESaveResult                Result = ESaveResult::Missing;
            TStrongObjectPtr<SaveGame> Save;
            OpaaxString                Error;
            TDynArray<OpaaxString>     BadFields;
        };

        /** A request the worker ran, back for the main thread. */
        struct Finished
        {
            Request Asked;
            Outcome Answer;
        };

        /** The requests, shared with the worker: a job still running finds them after the subsystem is gone. */
        struct RequestQueue
        {
            Mutex                      Lock;
            TQueue<Request>            Pending;
            TDynArray<Finished>        Done;
            bool                       bRunning = false;   // a worker job is taking the requests
            ISaveStorage*              Storage  = nullptr;
            const ObjectClassRegistry* Classes  = nullptr;
        };

        /** Load for a class known at run time. */
        ESaveResult LoadOfClass(OpaaxStringView InSlot, const ObjectClass& InClass,
                                TStrongObjectPtr<SaveGame>& OutSave);

        /** LoadAsync for a class known at run time. */
        void LoadOfClassAsync(OpaaxStringView InSlot, const ObjectClass& InClass, FOnLoadedSave InOnLoaded);

        /** Queues InRequest, and starts a worker job when none is taking the requests. */
        void Enqueue(Request InRequest);

        /** Waits until no worker job is taking requests. Main thread. */
        void WaitForRequests() const;

        /** Logs the finished requests and runs their callbacks, or drops them when bInRunCallbacks is false. */
        void DeliverFinished(bool bInRunCallbacks);

        /** The worker job: runs the queued requests in order until none is left. */
        static void RunRequests(const TSharedPtr<RequestQueue>& InQueue);

        /** Runs one request. Touches nothing but its arguments: runs on any thread. */
        static Outcome RunRequest(ISaveStorage& InStorage, const ObjectClassRegistry* InClasses,
                                  const Request& InRequest);

        /** Logs how a load ended. */
        static void ReportLoad(OpaaxStringView InSlot, const Outcome& InOutcome);

        /** Logs how a save ended. */
        static void ReportSave(OpaaxStringView InSlot, OpaaxStringID InClass, ESaveResult InResult);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TSharedPtr<RequestQueue> m_Queue;

        // The last worker job started: what WaitForRequests waits on.
        JobHandle m_Runner;
    };
}
