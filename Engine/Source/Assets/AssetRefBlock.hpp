#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"


namespace Opaax
{
    /**
     * @struct AssetRefBlock
     *
     * Shared ref-count block allocated once per loaded asset.
     * Both AssetRegistry (owner) and AssetHandle (borrower) hold a raw ptr to it.
     *      AssetHandle never deletes it — only the registry does, after the asset itself is destroyed and RefCount hits 0.
     *
     * RefCount is atomic — AssetHandle copies/destructs are safe from any thread. Registry Load/Unload must still be called from the main thread.
     *
     * AssetRegistry allocates and owns the block lifetime.
     */
    struct AssetRefBlock
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
        // Move - non-movable
        // =============================================================================
        
        AssetRefBlock(AssetRefBlock&&)                 = delete;
        AssetRefBlock& operator=(AssetRefBlock&&)      = delete;

        // =============================================================================
        // Function
        // =============================================================================

        //------------------------------------------------------------------------------
        //  Get - Set
        
        FORCEINLINE void AddRef() noexcept
        {
            RefCount.fetch_add(1, std::memory_order_relaxed);
        }

        // Returns the ref count BEFORE the decrement.
        FORCEINLINE Uint32 Release() noexcept
        {
            return RefCount.fetch_sub(1, std::memory_order_acq_rel);
        }

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