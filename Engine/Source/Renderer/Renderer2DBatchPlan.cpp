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
                         const QuadBatchLimits&    InLimits,
                         TDynArray<QuadPlacement>& OutPlan)
    {
        OutPlan.clear();

        const Uint32 lCount = static_cast<Uint32>(std::min(InKeys.size(), InTextureIds.size()));
        if (lCount == 0) { return; }

        const Uint32 lMaxQuads = std::max(InLimits.MaxQuads, 1u);
        const Uint32 lMaxSlots = std::max(InLimits.MaxTextureSlots, 2u);

        OutPlan.reserve(lCount);
        for (Uint32 i = 0; i < lCount; ++i) { OutPlan.emplace_back(i, 0u, 0u); }

        std::stable_sort(OutPlan.begin(), OutPlan.end(),
            [&InKeys](const QuadPlacement& InA, const QuadPlacement& InB)
            { return InKeys[InA.QuadIndex] < InKeys[InB.QuadIndex]; });

        TDynArray<Uint32> lSlotTexIds;   // index i = slot i + 1; slot 0 is white
        lSlotTexIds.reserve(lMaxSlots);

        Uint32 lBatch        = 0;
        Uint32 lQuadsInBatch = 0;

        for (QuadPlacement& lPlacement : OutPlan)
        {
            const Uint32 lTexId = InTextureIds[lPlacement.QuadIndex];

            Uint32     lSlot      = FindSlot(lSlotTexIds, lTexId);
            const bool lNeedsSlot = (lTexId != 0 && lSlot == 0);

            // Quad room first, then samplers: a full batch closes whatever texture comes next.
            if (lQuadsInBatch >= lMaxQuads
                || (lNeedsSlot && static_cast<Uint32>(lSlotTexIds.size()) + 1 >= lMaxSlots))
            {
                ++lBatch;
                lQuadsInBatch = 0;
                lSlotTexIds.clear();
                lSlot = 0;
            }

            if (lTexId != 0 && lSlot == 0)
            {
                lSlotTexIds.emplace_back(lTexId);
                lSlot = static_cast<Uint32>(lSlotTexIds.size());
            }

            lPlacement.Batch = lBatch;
            lPlacement.Slot  = lSlot;
            ++lQuadsInBatch;
        }
    }
}
