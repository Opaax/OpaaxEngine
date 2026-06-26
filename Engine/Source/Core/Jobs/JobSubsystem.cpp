#include "JobSubsystem.h"

#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    // =============================================================================
    // Lifecycle
    // =============================================================================

    bool JobSubsystem::Startup()
    {
        const Uint32 lHardware = static_cast<Uint32>(Thread::hardware_concurrency());

        // hardware_concurrency may report 0 when it can't detect the core count.
        const Uint32 lDetected = (lHardware > 0) ? lHardware : 1;
        const Uint32 lWorkers  = (lDetected > m_ReservedThreads) ? (lDetected - m_ReservedThreads) : 1;

        m_Workers.reserve(lWorkers);
        for (Uint32 i = 0; i < lWorkers; ++i)
        {
            m_Workers.emplace_back([this] { WorkerLoop(); });
        }

        OPAAX_CORE_INFO("JobSubsystem::Startup — {} worker(s) (hardware {}, reserved {})",
                        GetWorkerCount(), lDetected, m_ReservedThreads);
        return true;
    }

    void JobSubsystem::Update(double /*DeltaTime*/)
    {
        DrainCompleted();
    }

    void JobSubsystem::Shutdown()
    {
        {
            LockGuard<Mutex> lLock(m_QueueMutex);
            m_Stopping.store(true, std::memory_order_release);
        }
        m_QueueCV.notify_all();

        for (Thread& lWorker : m_Workers)
        {
            if (lWorker.joinable()) { lWorker.join(); }
        }
        m_Workers.clear();

        LockGuard<Mutex> lLock(m_CompletedMutex);
        if (!m_Completed.empty())
        {
            OPAAX_CORE_WARN("JobSubsystem::Shutdown — {} completion callback(s) never drained",
                            m_Completed.size());
        }
        m_Completed.clear();

        OPAAX_CORE_INFO("JobSubsystem::Shutdown — workers joined");
    }

    // =============================================================================
    // Submission
    // =============================================================================

    JobHandle JobSubsystem::Submit(TFunction<void()> InWork)
    {
        return Submit(Move(InWork), TFunction<void()>{});
    }

    JobHandle JobSubsystem::Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete)
    {
        SharedPtr<JobState> lState = MakeShared<JobState>();

        Job lJob;
        lJob.Work       = Move(InWork);
        lJob.OnComplete = Move(InOnComplete);
        lJob.State      = lState;

        {
            LockGuard<Mutex> lLock(m_QueueMutex);
            m_Queue.push(Move(lJob));
        }
        m_QueueCV.notify_one();

        return JobHandle{ Move(lState) };
    }

    void JobSubsystem::ParallelFor(Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32 InGrainSize)
    {
        if (InCount == 0)
        {
            return;
        }
        
        if (InGrainSize == 0)
        {
            InGrainSize = 1;
        }

        TDynArray<JobHandle> lHandles;
        lHandles.reserve(InCount / InGrainSize + 1);

        // Capture InBody by reference — every chunk completes before this function
        // returns (the Wait loop below), so the reference can't dangle.
        Uint32 lStart = 0;
        while (lStart < InCount)
        {
            const Uint32 lEnd    = (lStart + InGrainSize < InCount) ? (lStart + InGrainSize) : InCount;
            const bool   lIsLast = (lEnd == InCount);

            if (lIsLast)
            {
                // Run the final chunk on the calling thread instead of idling.
                for (Uint32 k = lStart; k < lEnd; ++k) { InBody(k); }
            }
            else
            {
                lHandles.push_back(Submit([&InBody, lStart, lEnd]
                {
                    for (Uint32 k = lStart; k < lEnd; ++k) { InBody(k); }
                }));
            }
            lStart = lEnd;
        }

        for (const JobHandle& lHandle : lHandles)
        {
            Wait(lHandle);
        }
    }

    void JobSubsystem::Wait(const JobHandle& InHandle)
    {
        const SharedPtr<JobState>& lState = InHandle.GetState();
        if (!lState) { return; }

        // NOTE: do not call from a worker thread — there is no work-stealing, so a
        // worker blocking here while all workers are busy would deadlock 
        UniqueLock<Mutex> lLock(m_DoneMutex);
        m_DoneCV.wait(lLock, [&lState] { return lState->bDone.load(std::memory_order_acquire); });
    }

    // =============================================================================
    // Internal
    // =============================================================================

    void JobSubsystem::WorkerLoop()
    {
        for (;;)
        {
            Job lJob;
            {
                UniqueLock<Mutex> lLock(m_QueueMutex);
                m_QueueCV.wait(lLock, [this]
                {
                    return m_Stopping.load(std::memory_order_acquire) || !m_Queue.empty();
                });

                // Drain remaining work even while stopping; only exit once empty.
                if (m_Queue.empty())
                {
                    return;
                }

                lJob = Move(m_Queue.front());
                m_Queue.pop();
            }

            if (lJob.Work)
            {
                lJob.Work();
            }

            // Flip the done-flag under m_DoneMutex so a concurrent Wait can't miss
            // the notify (store-then-notify with the waiter holding the same lock).
            {
                LockGuard<Mutex> lLock(m_DoneMutex);
                if (lJob.State)
                {
                    lJob.State->bDone.store(true, std::memory_order_release);
                }
            }
            m_DoneCV.notify_all();

            if (lJob.OnComplete)
            {
                LockGuard<Mutex> lLock(m_CompletedMutex);
                m_Completed.push_back(Move(lJob.OnComplete));
            }
        }
    }

    void JobSubsystem::DrainCompleted()
    {
        TDynArray<TFunction<void()>> lLocal;
        {
            LockGuard<Mutex> lLock(m_CompletedMutex);
            if (m_Completed.empty())
            {
                return;
            }
            lLocal.swap(m_Completed);
        }

        // Invoke outside the lock so a callback may safely Submit more work.
        for (TFunction<void()>& lCallback : lLocal)
        {
            if (lCallback)
            {
                lCallback();
            }
        }
    }

} // namespace Opaax
