#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // Batch plan — a pass's draw order, decided before anything reaches the GPU
    // =============================================================================

    /** What one batch can hold. From RenderLimits, at Renderer2D::Init. */
    struct QuadBatchLimits
    {
        Uint32 MaxQuads        = 1000;
        Uint32 MaxTextureSlots = 16;
    };

    /** Where one recorded quad goes: which batch, which sampler slot(s). */
    struct QuadPlacement
    {
        Uint32 QuadIndex = 0;   // index of the recorded quad
        Uint32 Batch     = 0;   // batch index
        Uint32 Slot      = 0;   // sampler slot in that batch
        Uint32 MaskSlot  = 0;   // mask slot; 0 = no mask
    };

    /**
     * Sorts a whole pass by draw-order key, then splits it into batches and assigns sampler slots.
     * Sorting first keeps the draw order across batches and groups quads by texture.
     * @param InKeys       One MakeSortKey per recorded quad
     * @param InTextureIds Same size as InKeys. 0 is the white texture (slot 0, free).
     * @param InMaskIds    Same size as InKeys, 0 = no mask. May be empty (no masks).
     *                     A masked quad may need two slots (one if the mask is its own texture).
     * @param InLimits     Per-batch capacity (at least 1 quad and 2 slots)
     * @param OutPlan      Filled in draw order. Cleared first (capacity kept).
     */
    OPAAX_API void PlanQuadBatches(const TDynArray<Uint64>&  InKeys,
                                   const TDynArray<Uint32>&  InTextureIds,
                                   const TDynArray<Uint32>&  InMaskIds,
                                   const QuadBatchLimits&    InLimits,
                                   TDynArray<QuadPlacement>& OutPlan);
}
