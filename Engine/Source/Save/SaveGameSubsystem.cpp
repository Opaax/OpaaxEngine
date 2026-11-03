#include "Save/SaveGameSubsystem.h"

#include "Application/Services/IJobSystem.h"
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
        WaitForRequests();

        Request lRequest;
        lRequest.Slot = InSlot.ToString();
        lRequest.Text = SaveGameFile::Serialize(InSave);

        const GameInstanceContext& lContext = GetContext();
        const Outcome lOutcome = RunRequest(lContext.Saves, lContext.Resources.GetObjectClasses(), lRequest);

        ReportSave(InSlot, InSave.GetClass().GetName(), lOutcome.Result);
        return lOutcome.Result;
    }

    bool SaveGameSubsystem::Exists(const OpaaxStringView InSlot) const
    {
        WaitForRequests();
        return GetContext().Saves.Exists(InSlot);
    }

    ESaveResult SaveGameSubsystem::Delete(const OpaaxStringView InSlot)
    {
        WaitForRequests();

        const ESaveResult lResult = GetContext().Saves.Delete(InSlot);
        if (lResult == ESaveResult::Ok)
        {
            OPAAX_LOG(LogSaveGame, Info, "Deleted slot '{}'", InSlot);
        }
        return lResult;
    }

    TDynArray<OpaaxString> SaveGameSubsystem::ListSlots() const
    {
        WaitForRequests();
        return GetContext().Saves.ListSlots();
    }

    ESaveResult SaveGameSubsystem::LoadOfClass(const OpaaxStringView InSlot, const ObjectClass& InClass,
                                               TStrongObjectPtr<SaveGame>& OutSave)
    {
        WaitForRequests();

        Request lRequest;
        lRequest.Slot      = InSlot.ToString();
        lRequest.LoadClass = &InClass;

        const GameInstanceContext& lContext = GetContext();
        Outcome lOutcome = RunRequest(lContext.Saves, lContext.Resources.GetObjectClasses(), lRequest);

        ReportLoad(InSlot, lOutcome);

        if (lOutcome.Result == ESaveResult::Ok)
        {
            OutSave = Move(lOutcome.Save);
        }
        return lOutcome.Result;
    }

    // =========================================================================
    // In the background
    // =========================================================================
    void SaveGameSubsystem::SaveAsync(const OpaaxStringView InSlot, const SaveGame& InSave, FOnSaved InOnSaved)
    {
        Request lRequest;
        lRequest.Slot      = InSlot.ToString();
        lRequest.Text      = SaveGameFile::Serialize(InSave);
        lRequest.SaveClass = InSave.GetClass().GetName();
        lRequest.OnSaved   = Move(InOnSaved);

        Enqueue(Move(lRequest));
    }

    void SaveGameSubsystem::LoadOfClassAsync(const OpaaxStringView InSlot, const ObjectClass& InClass,
                                             FOnLoadedSave InOnLoaded)
    {
        Request lRequest;
        lRequest.Slot      = InSlot.ToString();
        lRequest.LoadClass = &InClass;
        lRequest.OnLoaded  = Move(InOnLoaded);

        Enqueue(Move(lRequest));
    }

    void SaveGameSubsystem::Flush()
    {
        WaitForRequests();
        DeliverFinished(true);
    }

    bool SaveGameSubsystem::IsBusy() const
    {
        if (!m_Queue)
        {
            return false;
        }

        TLockGuard<Mutex> lLock(m_Queue->Lock);
        return m_Queue->bRunning || !m_Queue->Pending.empty() || !m_Queue->Done.empty();
    }

    // =========================================================================
    // Override
    // =========================================================================
    bool SaveGameSubsystem::Startup()
    {
        const GameInstanceContext& lContext = GetContext();

        m_Queue          = MakeShared<RequestQueue>();
        m_Queue->Storage = &lContext.Saves;
        m_Queue->Classes = lContext.Resources.GetObjectClasses();
        return true;
    }

    void SaveGameSubsystem::TearDown()
    {
        // The whole game is still alive: the callbacks may use it.
        Flush();
    }

    void SaveGameSubsystem::Shutdown()
    {
        // What was asked after TearDown is still written; its callbacks would reach a game already gone.
        WaitForRequests();
        DeliverFinished(false);
    }

    void SaveGameSubsystem::Update(double /*InDeltaTime*/)
    {
        DeliverFinished(true);
    }

    // =========================================================================
    // Internal
    // =========================================================================
    void SaveGameSubsystem::Enqueue(Request InRequest)
    {
        if (!m_Queue)
        {
            OPAAX_LOG(LogSaveGame, Error, "Slot '{}': the save subsystem has not started", InRequest.Slot.CStr());
            return;
        }

        bool bStartWorker = false;
        {
            TLockGuard<Mutex> lLock(m_Queue->Lock);
            m_Queue->Pending.push(Move(InRequest));
            bStartWorker      = !m_Queue->bRunning;
            m_Queue->bRunning = true;
        }

        // One worker job at a time takes the requests, in order: a load asked after a save reads it.
        if (bStartWorker)
        {
            m_Runner = GetContext().Jobs.Submit([lQueue = m_Queue]() { RunRequests(lQueue); });
        }
    }

    void SaveGameSubsystem::WaitForRequests() const
    {
        if (!m_Queue)
        {
            return;
        }

        for (;;)
        {
            {
                TLockGuard<Mutex> lLock(m_Queue->Lock);
                if (!m_Queue->bRunning)
                {
                    return;
                }
            }

            // A job system run by hand (a test's) gives no handle to wait on: its requests stay queued.
            if (!m_Runner.IsValid())
            {
                return;
            }

            GetContext().Jobs.Wait(m_Runner);
        }
    }

    void SaveGameSubsystem::DeliverFinished(const bool bInRunCallbacks)
    {
        if (!m_Queue)
        {
            return;
        }

        // Taken out first: a callback may ask for more.
        TDynArray<Finished> lFinished;
        {
            TLockGuard<Mutex> lLock(m_Queue->Lock);
            lFinished = Move(m_Queue->Done);
            m_Queue->Done.clear();
        }

        Uint32 lDropped = 0;
        for (const Finished& lFinish : lFinished)
        {
            const Request& lAsked = lFinish.Asked;

            if (lAsked.LoadClass != nullptr)
            {
                ReportLoad(lAsked.Slot, lFinish.Answer);
                if (lAsked.OnLoaded)
                {
                    if (bInRunCallbacks) { lAsked.OnLoaded(lFinish.Answer.Result, lFinish.Answer.Save); }
                    else                 { ++lDropped; }
                }
            }
            else
            {
                ReportSave(lAsked.Slot, lAsked.SaveClass, lFinish.Answer.Result);
                if (lAsked.OnSaved)
                {
                    if (bInRunCallbacks) { lAsked.OnSaved(lFinish.Answer.Result); }
                    else                 { ++lDropped; }
                }
            }
        }

        if (lDropped > 0)
        {
            OPAAX_LOG(LogSaveGame, Info, "{} save callback(s) not run: the game ended before their turn", lDropped);
        }
    }

    void SaveGameSubsystem::RunRequests(const TSharedPtr<RequestQueue>& InQueue)
    {
        for (;;)
        {
            Request lRequest;
            {
                TLockGuard<Mutex> lLock(InQueue->Lock);
                if (InQueue->Pending.empty())
                {
                    InQueue->bRunning = false;
                    return;
                }

                lRequest = Move(InQueue->Pending.front());
                InQueue->Pending.pop();
            }

            Outcome lOutcome = RunRequest(*InQueue->Storage, InQueue->Classes, lRequest);

            TLockGuard<Mutex> lLock(InQueue->Lock);
            InQueue->Done.push_back(Finished{ Move(lRequest), Move(lOutcome) });
        }
    }

    SaveGameSubsystem::Outcome SaveGameSubsystem::RunRequest(ISaveStorage& InStorage,
                                                             const ObjectClassRegistry* InClasses,
                                                             const Request& InRequest)
    {
        Outcome lOutcome;

        if (InRequest.LoadClass == nullptr)
        {
            lOutcome.Result = InStorage.Write(InRequest.Slot, InRequest.Text);
            return lOutcome;
        }

        OpaaxString lText;
        lOutcome.Result = InStorage.Read(InRequest.Slot, lText);
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

        // A class's own UpgradeProperties or PostLoad may throw: on a worker, that would end the program.
        try
        {
            lOutcome.Result = SaveGameFile::Instantiate(lContents, *InClasses, *InRequest.LoadClass,
                                                        OpaaxStringID(InRequest.Slot), lOutcome.Save,
                                                        &lOutcome.Error, &lOutcome.BadFields);
        }
        catch (...)
        {
            lOutcome        = Outcome{};
            lOutcome.Result = ESaveResult::Corrupt;
            lOutcome.Error  = OpaaxString("reading its properties threw an exception (see the class's "
                                          "UpgradeProperties and PostLoad)");
        }
        return lOutcome;
    }

    void SaveGameSubsystem::ReportLoad(const OpaaxStringView InSlot, const Outcome& InOutcome)
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

    void SaveGameSubsystem::ReportSave(const OpaaxStringView InSlot, const OpaaxStringID InClass,
                                       const ESaveResult InResult)
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
