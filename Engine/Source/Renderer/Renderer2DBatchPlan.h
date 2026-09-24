#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // Batch plan — one PASS's draw order, decided before anything reaches the GPU
    // =============================================================================

    /** What ONE flush can hold. Resolved from RenderLimits at Renderer2D::Init. */
    struct QuadBatchLimits
    {
        Uint32 MaxQuads        = 1000;
        Uint32 MaxTextureSlots = 16;
    };

    /** Where one recorded quad lands: which flush draws it, at which sampler slot(s). */
    struct QuadPlacement
    {
        Uint32 QuadIndex = 0;   // index into the pass's recorded quads
        Uint32 Batch     = 0;   // 0-based flush
        Uint32 Slot      = 0;   // sampler slot within that flush
        Uint32 MaskSlot  = 0;   // the MASK's slot in that flush; 0 when the quad has no mask (UI16)
    };

    /**
     * Sort a whole pass by draw-order key, THEN cut it into batches, handing out sampler slots as
     * it goes.
     *
     * The order of those two steps is the whole point: a batch boundary no longer decides draw
     * order, so the painter's algorithm holds across any number of flushes. Sorting first also
     * groups equal-key quads by texture before the slots are handed out, so the cut is tighter
     * than a submission-order one.
     *
     * Free and pure so the ordering is testable with no GL context — the same call as MakeSortKey
     * one level up.
     *
     * @param InKeys       one MakeSortKey per recorded quad.
     * @param InTextureIds parallel to InKeys. 0 is the white texture: it owns slot 0 and never
     *                     costs a slot. Any other id is a distinct texture that needs one.
     * @param InMaskIds    parallel to InKeys, 0 = no mask (**UI16**). A masked quad needs its MASK
     *                     bound too, so it may cost TWO slots — or ONE when the mask and the
     *                     texture are the same id, which is why they are resolved together. An
     *                     empty array is "nothing is masked" and plans exactly as it did before.
     * @param InLimits     per-batch capacity. Read as at least 1 quad and 2 slots (white + one
     *                     texture), so no input can produce a batch nothing fits in.
     * @param OutPlan      filled in EMIT order. Cleared first, capacity kept — the caller reuses
     *                     one across frames.
     */
    OPAAX_API void PlanQuadBatches(const TDynArray<Uint64>&  InKeys,
                                   const TDynArray<Uint32>&  InTextureIds,
                                   const TDynArray<Uint32>&  InMaskIds,
                                   const QuadBatchLimits&    InLimits,
                                   TDynArray<QuadPlacement>& OutPlan);
}
