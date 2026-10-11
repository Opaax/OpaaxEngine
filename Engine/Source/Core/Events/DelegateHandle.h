#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // DelegateHandle
    // =============================================================================

    /**
     * Unique id of one binding in a TMulticastDelegate or EventBus. Needed to remove it.
     * A default handle is invalid (id 0).
     */
    class DelegateHandle
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        DelegateHandle() noexcept = default;

    private:
        explicit DelegateHandle(Uint64 InID) noexcept : m_ID(InID) {}

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Creates a new unique handle. Thread-safe. */
        static DelegateHandle Generate() noexcept;

        // -----------------------------------------------------------------------------
        // Getters
    public:
        FORCEINLINE bool   IsValid() const noexcept { return m_ID != 0; }
        FORCEINLINE Uint64 GetID()   const noexcept { return m_ID; }

        // -----------------------------------------------------------------------------
        // Comparison
    public:
        FORCEINLINE bool operator==(const DelegateHandle& Other) const noexcept { return m_ID == Other.m_ID; }
        FORCEINLINE bool operator!=(const DelegateHandle& Other) const noexcept { return m_ID != Other.m_ID; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint64 m_ID = 0;
    };
}
