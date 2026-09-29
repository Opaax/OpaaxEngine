#include "IJobSystem.h"
#include "Core/Log/Logger.h"
#include "Application/OpaaxApplication.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullJobSystem — runs every job immediately on the calling thread.
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
                if (InOnComplete) { InOnComplete(); }
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
    // Type tag + null object (defined here so they are shared across the DLL/exe boundary).
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
            TLockGuard<Mutex> lLock(m_QueueMutex);
            if (m_Stopping.load(std::memory_order_acquire)) { return; } // already stopped
            m_Stopping.store(true, std::memory_order_release);
        }
        m_QueueCV.notify_all();

        for (Thread& lWorker : m_Workers)
        {
            if (lWorker.joinable()) { lWorker.join(); }
        }
        m_Workers.clear();

        TLockGuard<Mutex> lLock(m_CompletedMutex);
        if (!m_Completed.empty())
        {
            OPAAX_LOG(LogJobSystem, Warn, "{} completion callback(s) never drained", m_Completed.size());
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
        TSharedPtr<JobState> lState = MakeShared<JobState>();

        Job lJob;
        lJob.Work       = Move(InWork);
        lJob.OnComplete = Move(InOnComplete);
        lJob.State      = lState;

        {
            TLockGuard<Mutex> lLock(m_QueueMutex);
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

        // InBody is captured by reference: every chunk finishes before we return.
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
                lHandles.emplace_back(Submit([&InBody, lStart, lEnd]
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
        const TSharedPtr<JobState>& lState = InHandle.GetState();
        if (!lState) { return; }

        // Not from a worker thread: without work-stealing it could deadlock.
        TUniqueLock<Mutex> lLock(m_DoneMutex);
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
                TUniqueLock<Mutex> lLock(m_QueueMutex);
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

            // Set under the lock so a concurrent Wait cannot miss the notify.
            {
                TLockGuard<Mutex> lLock(m_DoneMutex);
                if (lJob.State)
                {
                    lJob.State->bDone.store(true, std::memory_order_release);
                }
            }
            m_DoneCV.notify_all();

            if (lJob.OnComplete)
            {
                TLockGuard<Mutex> lLock(m_CompletedMutex);
                m_Completed.emplace_back(Move(lJob.OnComplete));
            }
        }
    }

    void JobSystem::DrainCompletions()
    {
        TDynArray<TFunction<void()>> lLocal;
        {
            TLockGuard<Mutex> lLock(m_CompletedMutex);
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
