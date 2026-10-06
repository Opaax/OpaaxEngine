#pragma once

#include "IAppService.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Jobs/JobHandle.h"

namespace Opaax
{
    inline constexpr LogCategory LogJobSystem{"JobSystem"};
    
    // =============================================================================
    // IJobSystem — worker thread pool.
    //   Jobs run on a worker thread. The optional OnComplete callback runs on the main
    //   thread during the next DrainCompletions() (once per frame).
    // =============================================================================
    class IJobSystem : public IAppService
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
         * Queues work on a worker thread.
         * @return A handle to poll or Wait on
         */
        virtual JobHandle Submit(TFunction<void()> InWork) = 0;
        
        /**
         * Queues work on a worker thread. InOnComplete then runs on the main thread,
         * during the next DrainCompletions().
         * @return A handle to poll or Wait on
         */
        virtual JobHandle Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete) = 0;
        
        /**
         * Runs InBody over [0, InCount), split into InGrainSize chunks across the pool.
         * Blocks until every chunk is done.
         */
        virtual void ParallelFor(Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32 InGrainSize = 1) = 0;
        
        /**
         * Blocks until the job has run. Do not call from a worker thread.
         */
        virtual void Wait(const JobHandle& InHandle) = 0;

        /**
         * Runs the pending completion callbacks. Main thread only.
         */
        virtual void DrainCompletions() = 0;

        virtual Uint32 GetWorkerCount() const noexcept = 0;

        //----- null object ----------------------------------------------------
        static IJobSystem& Null();
    };

    // =============================================================================
    // JobSystem — spawns (core count - reserved) workers, at least 1.
    // =============================================================================
    class JobSystem final : public IJobSystem
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        // InReservedThreads: cores kept free for the main thread and other dedicated threads.
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
            TSharedPtr<JobState> State;
        };

        void WorkerLoop();

        /**
         * Stops and joins the workers. Safe to call twice.
         */
        void StopAndJoin();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<Thread> m_Workers;

        // Pending jobs.
        TQueue<Job>       m_Queue;
        Mutex             m_QueueMutex;
        ConditionVariable m_QueueCV;

        // Completion callbacks waiting for DrainCompletions.
        TDynArray<TFunction<void()>> m_Completed;
        Mutex                        m_CompletedMutex;

        // Used by Wait: notified each time a job finishes.
        Mutex             m_DoneMutex;
        ConditionVariable m_DoneCV;

        TAtomic<bool> m_Stopping{false};
    };
}
