#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // DelegateHandle
    // =============================================================================

    /**
     * @class DelegateHandle
     * Opaque, process-unique id for a single binding inside a TMulticastDelegate
     * (and, later, a subscription on the event bus). Returned by every Add/Subscribe;
     * required by the matching Remove/Unsubscribe. A default-constructed handle is
     * invalid (id 0) and matches nothing.
     */
    class OPAAX_API DelegateHandle
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
        /** Mint a fresh, process-unique handle. Thread-safe. */
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
