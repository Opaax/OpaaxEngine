#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Core/OpaaxTypes.h"

#include "JobHandle.h"

namespace Opaax
{
    /**
     * @class JobSubsystem
     *
     * Engine-owned worker thread pool. Off-thread work is submitted via Submit and
     * runs on one of the pool's worker threads; an optional OnComplete callback is
     * marshalled back onto the main thread during the per-frame drain (Update) so
     * GPU / main-thread-affine finalization stays safe.
     *
     * Ordering contract: this subsystem is registered FIRST among engine subsystems
     * (CoreEngineApp::Initialize). That gives it two properties for free —
     *   - Update() runs at the very start of each frame, so completion callbacks land
     *     before any other system ticks (completions are visible the same frame), and
     *   - reverse-order ShutdownAll() tears it down LAST, so its worker join happens
     *     after every other subsystem has stopped issuing jobs.
     *
     * Not play-only — the pool exists in editor and play alike.
     */
    class OPAAX_API JobSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(JobSubsystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        JobSubsystem() = default;
        explicit JobSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        ~JobSubsystem() override = default;

        JobSubsystem(const JobSubsystem&)            = delete;
        JobSubsystem& operator=(const JobSubsystem&) = delete;
        JobSubsystem(JobSubsystem&&)                 = default;
        JobSubsystem& operator=(JobSubsystem&&)      = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Queue work to run on a worker thread.
         * @return a handle to poll/wait for completion.
         */
        JobHandle Submit(TFunction<void()> InWork);

        /**
         * Queue work to run on a worker thread; InOnComplete is invoked on the MAIN
         * thread during the next DrainCompleted (Update). Use this for any result
         * handoff that must touch main-thread-affine state (GPU uploads, registries).
         */
        JobHandle Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete);

        /**
         * Run InBody over [0, InCount) split into InGrainSize chunks across the pool.
         * Blocks until every chunk finishes. The calling thread runs the last chunk
         * itself rather than idling. Safe to call with no workers (runs inline).
         */
        void ParallelFor(Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32 InGrainSize = 1);

        /** Block until the job behind InHandle has run. No-op for a null/complete handle. */
        void Wait(const JobHandle& InHandle);

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        Uint32 GetWorkerCount() const noexcept { return static_cast<Uint32>(m_Workers.size()); }

        /**
         * Cores reserved for the main thread and future dedicated long-lived threads
         * (render, physics, AI, animation). Worker count = hardware_concurrency minus
         * this, clamped to >= 1. Set BEFORE Startup to take effect. Defaults to 1
         * (main thread only) — bump it as those dedicated threads come online.
         */
        void   SetReservedThreads(Uint32 InCount) noexcept { m_ReservedThreads = InCount; }
        Uint32 GetReservedThreads() const noexcept { return m_ReservedThreads; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()              override;
        void Update(double DeltaTime) override;
        void Shutdown()             override;

        bool IsPlayOnly() const noexcept override { return false; }
        //~End EngineSubsystemBase Interface

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

        void WorkerLoop();

        /** Invoke queued main-thread completion callbacks. Main thread only. */
        void DrainCompleted();

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

        // Backs JobHandle::Wait — notified each time a job flips its done-flag.
        Mutex             m_DoneMutex;
        ConditionVariable m_DoneCV;

        Atomic<bool> m_Stopping{false};
        Uint32       m_ReservedThreads = 1;
    };

} // namespace Opaax
