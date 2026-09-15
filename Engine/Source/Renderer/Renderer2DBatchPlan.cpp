#include "Renderer2DBatchPlan.h"

#include <algorithm>

namespace Opaax
{
    namespace
    {
        // The slot InTexId already holds in this batch, or 0 — which doubles as "not bound yet",
        // since slot 0 is the white texture and never appears in the table.
        Uint32 FindSlot(const TDynArray<Uint32>& InSlotTexIds, const Uint32 InTexId)
        {
            for (Uint32 i = 0; i < static_cast<Uint32>(InSlotTexIds.size()); ++i)
            {
                if (InSlotTexIds[i] == InTexId) { return i + 1; }
            }

            return 0;
        }
    }

    void PlanQuadBatches(const TDynArray<Uint64>&  InKeys,
                         const TDynArray<Uint32>&  InTextureIds,
                         const TDynArray<Uint32>&  InMaskIds,
                         const QuadBatchLimits&    InLimits,
                         TDynArray<QuadPlacement>& OutPlan)
    {
        OutPlan.clear();

        const Uint32 lCount = static_cast<Uint32>(std::min(InKeys.size(), InTextureIds.size()));
        if (lCount == 0) { return; }

        const Uint32 lMaxQuads = std::max(InLimits.MaxQuads, 1u);
        const Uint32 lMaxSlots = std::max(InLimits.MaxTextureSlots, 2u);

        OutPlan.reserve(lCount);
        for (Uint32 i = 0; i < lCount; ++i) { OutPlan.emplace_back(i, 0u, 0u, 0u); }

        std::stable_sort(OutPlan.begin(), OutPlan.end(),
            [&InKeys](const QuadPlacement& InA, const QuadPlacement& InB)
            { return InKeys[InA.QuadIndex] < InKeys[InB.QuadIndex]; });

        TDynArray<Uint32> lSlotTexIds;   // index i = slot i + 1; slot 0 is white
        lSlotTexIds.reserve(lMaxSlots);

        Uint32 lBatch        = 0;
        Uint32 lQuadsInBatch = 0;

        const bool lHasMasks = !InMaskIds.empty();

        for (QuadPlacement& lPlacement : OutPlan)
        {
            const Uint32 lTexId  = InTextureIds[lPlacement.QuadIndex];
            const Uint32 lMaskId = (lHasMasks && lPlacement.QuadIndex < InMaskIds.size())
                                       ? InMaskIds[lPlacement.QuadIndex] : 0u;

            Uint32 lSlot     = FindSlot(lSlotTexIds, lTexId);
            Uint32 lMaskSlot = (lMaskId != 0) ? FindSlot(lSlotTexIds, lMaskId) : 0u;

            // How many NEW slots this quad would claim. A mask that is the same texture as the
            // quad's own shares one slot, which is why they are counted together rather than
            // checked one after the other (**UI16**).
            Uint32 lNeeded = (lTexId != 0 && lSlot == 0) ? 1u : 0u;
            if (lMaskId != 0 && lMaskSlot == 0 && lMaskId != lTexId) { ++lNeeded; }

            // Quad room first, then samplers: a full batch closes whatever texture comes next.
            if (lQuadsInBatch >= lMaxQuads
                || (lNeeded > 0 && static_cast<Uint32>(lSlotTexIds.size()) + lNeeded >= lMaxSlots))
            {
                ++lBatch;
                lQuadsInBatch = 0;
                lSlotTexIds.clear();
                lSlot     = 0;
                lMaskSlot = 0;
            }

            if (lTexId != 0 && lSlot == 0)
            {
                lSlotTexIds.emplace_back(lTexId);
                lSlot = static_cast<Uint32>(lSlotTexIds.size());
            }

            if (lMaskId != 0)
            {
                // Re-asked AFTER the texture was bound: when the two ids match, the mask rides the
                // slot the texture just took.
                lMaskSlot = FindSlot(lSlotTexIds, lMaskId);

                if (lMaskSlot == 0)
                {
                    lSlotTexIds.emplace_back(lMaskId);
                    lMaskSlot = static_cast<Uint32>(lSlotTexIds.size());
                }
            }

            lPlacement.Batch    = lBatch;
            lPlacement.Slot     = lSlot;
            lPlacement.MaskSlot = lMaskSlot;
            ++lQuadsInBatch;
        }
    }
}
