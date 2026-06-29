#include "IJobSystem.h"
#include "ILogger.h"
#include "Core/Application/OpaaxApplication.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullJobSystem — degrades by running every job INLINE on the calling
        // thread (no workers, no marshalling). Callers never branch on validity.
        // =====================================================================
        class NullJobSystem final : public IJobSystem
        {
        public:
            bool IsNull() const noexcept override { return true; }

            JobHandle Submit(TFunction<void()> InWork) override
            {
                if (InWork) { InWork(); }
                return JobHandle{}; // null handle reports complete
            }

            JobHandle Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete) override
            {
                if (InWork)       { InWork(); }
                if (InOnComplete) { InOnComplete(); } // no main-thread drain — run inline now
                return JobHandle{};
            }

            void ParallelFor(Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32 /*InGrainSize*/) override
            {
                for (Uint32 i = 0; i < InCount; ++i) { if (InBody) { InBody(i); } }
            }

            void   Wait(const JobHandle&) override {}
            void   DrainCompletions()     override {}
            Uint32 GetWorkerCount() const noexcept override { return 0; }
        };
    }

    // =========================================================================
    // Type tag + null object (out-of-line — one instance across the DLL/exe line).
    // =========================================================================
    ServiceTypeID IJobSystem::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IJobSystem& IJobSystem::Null()
    {
        static NullJobSystem s_Null;
        return s_Null;
    }

    // =========================================================================
    // JobSystem — lifecycle
    // =========================================================================
    JobSystem::JobSystem(Uint32 InReservedThreads)
    {
        const Uint32 lHardware = static_cast<Uint32>(Thread::hardware_concurrency());

        // hardware_concurrency may report 0 when it can't detect the core count.
        const Uint32 lDetected = (lHardware > 0) ? lHardware : 1;
        const Uint32 lWorkers  = (lDetected > InReservedThreads) ? (lDetected - InReservedThreads) : 1;

        m_Workers.reserve(lWorkers);
        for (Uint32 i = 0; i < lWorkers; ++i)
        {
            m_Workers.emplace_back([this] { WorkerLoop(); });
        }
        
        OPAAX_LOG(LogJobSystem, Info, "JobSystem — '{}' worker(s) (hardware '{}', reserved '{}')", GetWorkerCount(), lDetected, InReservedThreads);
    }

    JobSystem::~JobSystem()
    {
        StopAndJoin();
    }

    void JobSystem::OnShutdown()
    {
        StopAndJoin();
    }

    void JobSystem::StopAndJoin()
    {
        {
            LockGuard<Mutex> lLock(m_QueueMutex);
            if (m_Stopping.load(std::memory_order_acquire)) { return; } // already stopped — idempotent
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
            OPAAX_CORE_WARN("JobSystem — {} completion callback(s) never drained", m_Completed.size());
        }
        m_Completed.clear();
    }

    // =========================================================================
    // Submission
    // =========================================================================
    JobHandle JobSystem::Submit(TFunction<void()> InWork)
    {
        return Submit(Move(InWork), TFunction<void()>{});
    }

    JobHandle JobSystem::Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete)
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

    void JobSystem::ParallelFor(Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32 InGrainSize)
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

    void JobSystem::Wait(const JobHandle& InHandle)
    {
        const SharedPtr<JobState>& lState = InHandle.GetState();
        if (!lState) { return; }

        // NOTE: do not call from a worker thread — there is no work-stealing, so a
        // worker blocking here while all workers are busy would deadlock.
        UniqueLock<Mutex> lLock(m_DoneMutex);
        m_DoneCV.wait(lLock, [&lState] { return lState->bDone.load(std::memory_order_acquire); });
    }

    // =========================================================================
    // Internal
    // =========================================================================
    void JobSystem::WorkerLoop()
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

            // Flip the done-flag under m_DoneMutex so a concurrent Wait can't miss the
            // notify (store-then-notify with the waiter holding the same lock).
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

    void JobSystem::DrainCompletions()
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
}
