#pragma once

#include "IAppService.h"
#include "ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Jobs/JobHandle.h"

namespace Opaax
{
    inline constexpr LogCategory LogJobSystem{"JobSystem"};
    
    // =============================================================================
    // IJobSystem — process-lifetime worker-thread pool, exposed as an app service.
    //
    // Off-thread work is submitted via Submit and runs on a pool worker thread; an
    // optional OnComplete callback is marshalled back onto the MAIN thread during the
    // next DrainCompletions() (the engine loop pumps it once per frame) so GPU /
    // main-thread-affine finalization stays safe. Get<IJobSystem>() NEVER returns null —
    // the null object runs every job inline on the calling thread.
    // =============================================================================
    class OPAAX_API IJobSystem : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IJobSystem)

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Queue work to run on a worker thread. Returns a handle to poll / Wait on.
         * @param InWork 
         * @return 
         */
        virtual JobHandle Submit(TFunction<void()> InWork) = 0;
        
        /**
         * Queue work on a worker thread; InOnComplete runs on the MAIN thread during the next DrainCompletions(). Use for any result handoff touching main-thread state.
         * @param InWork 
         * @param InOnComplete 
         * @return 
         */
        virtual JobHandle Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete) = 0;
        
        /**
         * Run InBody over [0, InCount) split into InGrainSize chunks across the pool.
         * Blocks until every chunk finishes (the caller runs the last chunk itself).
         * Safe with no workers (runs inline).
         * @param InCount 
         * @param InBody 
         * @param InGrainSize 
         */
        virtual void ParallelFor(Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32 InGrainSize = 1) = 0;
        
        /**
         * Block until the job behind InHandle has run. No-op for a null/complete handle.
         * @param InHandle 
         */
        virtual void Wait(const JobHandle& InHandle) = 0;

        /**
         * Invoke queued main-thread completion callbacks. MAIN thread only — the engine loop calls this once per frame.
         */
        virtual void DrainCompletions() = 0;

        /***/
        virtual Uint32 GetWorkerCount() const noexcept = 0;

        //----- null object ----------------------------------------------------
        static IJobSystem& Null();
    };

    // =============================================================================
    // JobSystem — the real pool. Spawns (hardware_concurrency - reserved) workers at
    // construction (clamped >= 1) and joins them at teardown.
    // =============================================================================
    class OPAAX_API JobSystem final : public IJobSystem
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        // InReservedThreads: cores held back for the main thread + future dedicated
        // long-lived threads (render / physics / AI). Worker count = hardware minus this,
        // clamped to >= 1. Defaults to 1 (main thread only).
        explicit JobSystem(Uint32 InReservedThreads = 1);
        ~JobSystem() override;

        JobSystem(const JobSystem&)            = delete;
        JobSystem& operator=(const JobSystem&) = delete;
        JobSystem(JobSystem&&)                 = delete;
        JobSystem& operator=(JobSystem&&)      = delete;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Opaax::IAppService interface
    public:
        void OnShutdown() override;
        //~End Opaax::IAppService interface

        //~Begin Opaax::IJobSystem interface
    public:
        JobHandle Submit(TFunction<void()> InWork) override;
        JobHandle Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete) override;
        void      ParallelFor(Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32 InGrainSize) override;
        void      Wait(const JobHandle& InHandle) override;
        void      DrainCompletions() override;
        Uint32    GetWorkerCount() const noexcept override { return static_cast<Uint32>(m_Workers.size()); }
        //~End Opaax::IJobSystem interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        struct Job
        {
            TFunction<void()>   Work;
            TFunction<void()>   OnComplete;
            SharedPtr<JobState> State;
        };

        /***/
        void WorkerLoop();

        /**
         * Stop + join workers. Idempotent (m_Stopping-guarded) — both ~JobSystem() and
         * OnShutdown() call it, so a stack instance and the locator teardown both clean up.
         */
        void StopAndJoin();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<Thread> m_Workers;

        // Pending work queue — producers (Submit) push, workers pop.
        TQueue<Job>       m_Queue;
        Mutex             m_QueueMutex;
        ConditionVariable m_QueueCV;

        // Completion callbacks awaiting the main-thread drain.
        TDynArray<TFunction<void()>> m_Completed;
        Mutex                        m_CompletedMutex;

        // Backs Wait — notified each time a job flips its done-flag.
        Mutex             m_DoneMutex;
        ConditionVariable m_DoneCV;

        Atomic<bool> m_Stopping{false};
    };
}
