#pragma once

#include "Core/OpaaxTypes.h"   // Uint32 / Uint64

namespace Opaax
{
    // =============================================================================
    // Frame batch emitter (pure)
    // =============================================================================

    //------------------------------------------------------------------------------
    // Upper bound on slots a single batch can hold. The runtime cap (MAX_TEXTURE_SLOTS,
    // 16 on GL 3.3) is always <= this; the constant only sizes the stack scratch so the
    // function never heap-allocates.
    constexpr Uint32 k_MaxBatchSlots = 64;

    /**
     * Per-command batch assignment produced by AssignBatches.
     */
    struct BatchAssignment
    {
        Uint32 BatchIndex; // which emitted batch this command lands in
        Uint32 Slot;       // texture slot within that batch (0 = white)
    };

    /**
     * Assign already-sorted draw commands to batches + per-batch texture slots.
     *
     * The caller pre-sorts commands by draw key; this walks them in that order and groups them into
     * batches without ever reordering. Slot 0 is reserved for the white texture: a key of 0 maps to
     * slot 0 and never consumes a user slot. A batch closes (a new one opens) when the next command
     * would exceed InMaxQuadsPerBatch quads OR needs a (InMaxSlots+1)-th distinct texture. Because the
     * walk never reorders, concatenating the batches in index order reproduces the input draw order —
     * so a frame-global sort done before this call stays globally correct across the batch splits.
     *
     * Pure: no RHI, no Texture2D. Texture identity is an opaque Uint64 (the renderer passes the
     * texture pointer bits; the tests pass synthetic ids), so this is unit-testable in isolation.
     *
     * @param InSortedTexKeys    texture identity per command, in final draw order (0 = white)
     * @param InCount            number of commands
     * @param InMaxQuadsPerBatch per-batch quad cap (MAX_QUADS)
     * @param InMaxSlots         total slots incl. white (MAX_TEXTURE_SLOTS); clamped to [1, k_MaxBatchSlots]
     * @param OutAssign          [out] InCount entries, one per command
     * @return number of batches emitted (0 when InCount == 0)
     */
    //------------------------------------------------------------------------------
    inline Uint32 AssignBatches(const Uint64*    InSortedTexKeys,
                                Uint32           InCount,
                                Uint32           InMaxQuadsPerBatch,
                                Uint32           InMaxSlots,
                                BatchAssignment* OutAssign)
    {
        if (InCount == 0) { return 0; }

        if (InMaxSlots > k_MaxBatchSlots) { InMaxSlots = k_MaxBatchSlots; }
        if (InMaxSlots < 1)               { InMaxSlots = 1; }            // white slot is mandatory
        if (InMaxQuadsPerBatch < 1)       { InMaxQuadsPerBatch = 1; }

        Uint64 lSlotKeys[k_MaxBatchSlots];

        Uint32 lBatch       = 0;
        Uint32 lQuadInBatch = 0;
        Uint32 lSlotCount   = 1;   // slot 0 = white, always present
        Uint32 lLastSlot    = 0;   // last-hit cache (runs of the same texture are the common case)
        Uint64 lLastKey     = 0;
        lSlotKeys[0]        = 0;

        for (Uint32 i = 0; i < InCount; ++i)
        {
            const Uint64 lKey = InSortedTexKeys[i];

            // --- Resolve which slot this key occupies in the CURRENT batch (if any) ---
            Uint32 lSlot         = 0;
            bool   lNeedsNewSlot = false;
            if (lKey == 0)
            {
                lSlot = 0;                       // white never consumes a user slot
            }
            else if (lKey == lLastKey)
            {
                lSlot = lLastSlot;               // last-hit cache
            }
            else
            {
                bool lFound = false;
                for (Uint32 s = 1; s < lSlotCount; ++s)
                {
                    if (lSlotKeys[s] == lKey) { lSlot = s; lFound = true; break; }
                }
                lNeedsNewSlot = !lFound;
            }

            // --- Close the batch if this command no longer fits (quad cap or slot cap) ---
            const bool lQuadFull = (lQuadInBatch >= InMaxQuadsPerBatch);
            const bool lSlotFull = (lNeedsNewSlot && lSlotCount >= InMaxSlots);
            if (lQuadFull || lSlotFull)
            {
                ++lBatch;
                lQuadInBatch  = 0;
                lSlotCount    = 1;
                lLastKey      = 0;
                lLastSlot     = 0;
                lNeedsNewSlot = (lKey != 0);     // a non-white key needs a fresh slot in the new batch
                lSlot         = 0;
            }

            // --- Allocate a new slot for a first-seen texture in this batch ---
            if (lNeedsNewSlot)
            {
                lSlot                = lSlotCount;
                lSlotKeys[lSlotCount] = lKey;
                ++lSlotCount;
            }

            OutAssign[i].BatchIndex = lBatch;
            OutAssign[i].Slot       = lSlot;
            ++lQuadInBatch;

            lLastKey  = lKey;
            lLastSlot = lSlot;
        }

        return lBatch + 1;
    }
}
