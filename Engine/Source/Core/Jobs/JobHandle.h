#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // JobState
    // =============================================================================

    /**
     * State shared between a queued job and its handles. bDone is set once the job has run.
     */
    struct JobState
    {
        TAtomic<bool> bDone{false};
    };

    // =============================================================================
    // JobHandle
    // =============================================================================

    /**
     * Copyable handle to a submitted job. Carries no result (use the lambda captures).
     * A null handle counts as complete.
     */
    class OPAAX_API JobHandle
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        JobHandle() = default;
        explicit JobHandle(TSharedPtr<JobState> InState) : m_State(Move(InState)) {}

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
        const TSharedPtr<JobState>& GetState() const noexcept { return m_State; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TSharedPtr<JobState> m_State;
    };

} // namespace Opaax
