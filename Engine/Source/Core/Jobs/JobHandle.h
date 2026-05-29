#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // JobState
    // =============================================================================

    /**
     * @struct JobState
     *
     * Heap control block shared between a queued job and every JobHandle that
     * refers to it. The worker thread flips bDone once the job's work has run;
     * handles observe completion through it. Lifetime is ref-counted via the
     * SharedPtr the handle holds, so the block outlives both the job and the
     * handle independently.
     */
    struct JobState
    {
        Atomic<bool> bDone{false};
    };

    // =============================================================================
    // JobHandle
    // =============================================================================

    /**
     * @class JobHandle
     *
     * Lightweight, copyable observer of a single submitted job. Carries no result
     * payload by design (D-b) — results flow through the work/OnComplete lambda
     * captures. A default-constructed (null) handle reports complete and waits as
     * a no-op, so callers never branch on validity.
     *
     * Wait() routes through JobSubsystem so the completion condition variable stays
     * owned by the pool; the handle itself only holds the shared state.
     */
    class OPAAX_API JobHandle
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        JobHandle() = default;
        explicit JobHandle(SharedPtr<JobState> InState) : m_State(Move(InState)) {}

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** True once the worker has run the job's work (or the handle is null). */
        bool IsComplete() const noexcept
        {
            return !m_State || m_State->bDone.load(std::memory_order_acquire);
        }

        /** True when this handle refers to a real submitted job. */
        bool IsValid() const noexcept { return static_cast<bool>(m_State); }

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        const SharedPtr<JobState>& GetState() const noexcept { return m_State; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        SharedPtr<JobState> m_State;
    };

} // namespace Opaax
