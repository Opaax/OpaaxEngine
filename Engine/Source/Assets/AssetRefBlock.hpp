#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"


namespace Opaax
{
    /**
     * @struct AssetRefBlock
     *
     * Intrusive ref-counted lifetime block, allocated once per loaded asset.
     * Both AssetRegistry's AssetEntry (1 ref) and every live TAssetHandle (1 ref each)
     * hold the block. Release() self-deletes the block on the last drop, so the
     * block always outlives its observers even if AssetRegistry erases the entry
     * while handles are still alive.
     *
     * Always heap-allocate via `new AssetRefBlock`; never stack-allocate. AddRef and
     * Release MUST be balanced. Release is defined in AssetRefBlock.cpp so the matching
     * `delete this` always runs in the engine module — cross-module heap mismatch
     * would be UB.
     *
     * RefCount is atomic. Handle copies/destructs are safe from any thread.
     * Registry mutations (Load/Unload/Reload/Shutdown) must still be main-thread.
     */
    struct OPAAX_API AssetRefBlock
    {
        // =============================================================================
        // CTOR
        // =============================================================================

        AssetRefBlock()                                = default;

        // =============================================================================
        // Copy - Non-copyable
        // =============================================================================

        AssetRefBlock(const AssetRefBlock&)            = delete;
        AssetRefBlock& operator=(const AssetRefBlock&) = delete;

        // =============================================================================
        // Move - Non-movable
        // =============================================================================

        AssetRefBlock(AssetRefBlock&&)                 = delete;
        AssetRefBlock& operator=(AssetRefBlock&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================

        //------------------------------------------------------------------------------
        //  Get - Set

        FORCEINLINE void AddRef() noexcept
        {
            RefCount.fetch_add(1, std::memory_order_relaxed);
        }

        /*** Decrement the ref count; self-deletes when the last ref is released. */
        void Release() noexcept;

        FORCEINLINE Uint32 Get() const noexcept
        {
            return RefCount.load(std::memory_order_relaxed);
        }

        // =============================================================================
        // Members
        // =============================================================================

        Atomic<Uint32> RefCount { 0 };
    };

} // namespace Opaax
