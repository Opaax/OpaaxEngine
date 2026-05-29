#include "JobSubsystem.h"

#include "Core/Log/OpaaxLog.h"

// Step-4 verify scaffold. Removed at M6 close (Step 5).
#ifndef OPAAX_M6_VERIFY_JOBS
    #define OPAAX_M6_VERIFY_JOBS 1
#endif

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

#if OPAAX_M6_VERIFY_JOBS
        static bool s_VerifyKicked = false;
        if (!s_VerifyKicked)
        {
            s_VerifyKicked = true;

            // Update() always runs on the main thread — capture its id as the baseline
            // the completion callback below checks against.
            const Thread::id lMainId = std::this_thread::get_id();

            OPAAX_CORE_INFO("M6 VERIFY — workers={}", GetWorkerCount());

            // (a) ParallelFor correctness — square every index, compare against a
            //     single-threaded reference sum.
            constexpr Uint32 lN = 1u << 20; // 1,048,576
            TDynArray<Uint64> lOut(lN, 0);
            ParallelFor(lN, [&lOut](Uint32 InIndex)
            {
                lOut[InIndex] = static_cast<Uint64>(InIndex) * InIndex;
            }, /*grain*/ 4096);

            Uint64 lParallelSum = 0;
            Uint64 lReferenceSum = 0;
            for (Uint32 i = 0; i < lN; ++i)
            {
                lParallelSum  += lOut[i];
                lReferenceSum += static_cast<Uint64>(i) * i;
            }
            OPAAX_CORE_INFO("M6 VERIFY — ParallelFor {} (sum={}, ref={})",
                            (lParallelSum == lReferenceSum) ? "PASS" : "FAIL",
                            lParallelSum, lReferenceSum);

            // (b) Completion callback must run on the MAIN thread (next-frame drain).
            Submit(
                [] { /* off-thread work — nothing observable, just exercises the path */ },
                [lMainId]
                {
                    const bool lOnMain = (std::this_thread::get_id() == lMainId);
                    OPAAX_CORE_INFO("M6 VERIFY — OnComplete ran on {} thread",
                                    lOnMain ? "MAIN" : "WORKER(!)");
                });

            // (c) Submit + Wait — compute a value off-thread and block for it on the
            //     calling (main) thread. The shared result is captured by value so it
            //     outlives the job regardless of when the worker runs it.
            {
                SharedPtr<Uint64> lResult = MakeShared<Uint64>(0);
                JobHandle lHandle = Submit([lResult]
                {
                    Uint64 lAcc = 0;
                    for (Uint32 i = 0; i < 1'000'000; ++i) { lAcc += i; }
                    *lResult = lAcc;
                });
                Wait(lHandle);
                OPAAX_CORE_INFO("M6 VERIFY — Submit+Wait result={} (expected 499999500000)", *lResult);
            }

            // (d) The async-asset shape: heavy "decode" off-thread fills a CPU buffer,
            //     then the MAIN-thread completion "finalizes" it (where a real loader
            //     would do the GL upload). This is the exact pattern the future
            //     AssetRegistry::LoadAsync will ride on.
            {
                SharedPtr<TDynArray<Uint8>> lPayload = MakeShared<TDynArray<Uint8>>();
                Submit(
                    [lPayload]
                    {
                        lPayload->resize(256);
                        for (Uint32 i = 0; i < lPayload->size(); ++i)
                        {
                            (*lPayload)[i] = static_cast<Uint8>(i);
                        }
                    },
                    [lPayload, lMainId]
                    {
                        const bool lOnMain = (std::this_thread::get_id() == lMainId);
                        OPAAX_CORE_INFO("M6 VERIFY — async finalize: {} bytes ready, last={}, on {} thread",
                                        lPayload->size(),
                                        lPayload->empty() ? 0 : lPayload->back(),
                                        lOnMain ? "MAIN" : "WORKER(!)");
                    });
            }

            // (e) Fan-out — several independent jobs, each posting its own completion.
            //     All callbacks drain on the main thread in completion order.
            for (Uint32 lJobIndex = 0; lJobIndex < 4; ++lJobIndex)
            {
                Submit(
                    [lJobIndex]
                    {
                        // Uneven work so completion order isn't simply submission order.
                        volatile Uint64 lSpin = 0;
                        for (Uint32 i = 0; i < (lJobIndex + 1) * 200'000; ++i) { lSpin += i; }
                    },
                    [lJobIndex] { OPAAX_CORE_INFO("M6 VERIFY — fan-out job {} completed", lJobIndex); });
            }

            // (f) Chained submit — a completion callback queues a follow-up job. Proves
            //     DrainCompleted invokes callbacks OUTSIDE the completion lock, so a
            //     callback can safely Submit without re-entrant deadlock.
            Submit(
                [] {},
                [this, lMainId]
                {
                    OPAAX_CORE_INFO("M6 VERIFY — completion re-submitting a follow-up job");
                    Submit(
                        [] {},
                        [lMainId]
                        {
                            const bool lOnMain = (std::this_thread::get_id() == lMainId);
                            OPAAX_CORE_INFO("M6 VERIFY — chained follow-up completed on {} thread",
                                            lOnMain ? "MAIN" : "WORKER(!)");
                        });
                });
        }
#endif
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
        if (InCount == 0) { return; }
        if (InGrainSize == 0) { InGrainSize = 1; }

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

        for (const JobHandle& lHandle : lHandles) { Wait(lHandle); }
    }

    void JobSubsystem::Wait(const JobHandle& InHandle)
    {
        const SharedPtr<JobState>& lState = InHandle.GetState();
        if (!lState) { return; }

        // NOTE: do not call from a worker thread — there is no work-stealing, so a
        // worker blocking here while all workers are busy would deadlock (OD-2: v1
        // has no help-execute; revisit if a job ever waits on a sub-job).
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
                if (m_Queue.empty()) { return; }

                lJob = Move(m_Queue.front());
                m_Queue.pop();
            }

            if (lJob.Work) { lJob.Work(); }

            // Flip the done-flag under m_DoneMutex so a concurrent Wait can't miss
            // the notify (store-then-notify with the waiter holding the same lock).
            {
                LockGuard<Mutex> lLock(m_DoneMutex);
                if (lJob.State) { lJob.State->bDone.store(true, std::memory_order_release); }
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
            if (m_Completed.empty()) { return; }
            lLocal.swap(m_Completed);
        }

        // Invoke outside the lock so a callback may safely Submit more work.
        for (TFunction<void()>& lCallback : lLocal)
        {
            if (lCallback) { lCallback(); }
        }
    }

} // namespace Opaax
