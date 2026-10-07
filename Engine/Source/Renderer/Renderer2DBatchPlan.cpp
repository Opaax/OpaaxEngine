#include "Renderer/Renderer2DBatchPlan.h"

#include <algorithm>

namespace Opaax
{
    namespace
    {
        // InTexId's slot in this batch, or 0 if not bound (slot 0 is white, never in the table).
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
        PlanQuadBatches(InKeys, InTextureIds, InMaskIds, {}, InLimits, OutPlan);
    }

    void PlanQuadBatches(const TDynArray<Uint64>&  InKeys,
                         const TDynArray<Uint32>&  InTextureIds,
                         const TDynArray<Uint32>&  InMaskIds,
                         const TDynArray<Uint32>&  InNormalIds,
                         const QuadBatchLimits&    InLimits,
                         TDynArray<QuadPlacement>& OutPlan)
    {
        OutPlan.clear();

        const Uint32 lCount = static_cast<Uint32>(std::min(InKeys.size(), InTextureIds.size()));
        if (lCount == 0) { return; }

        const Uint32 lMaxQuads = std::max(InLimits.MaxQuads, 1u);
        const Uint32 lMaxSlots = std::max(InLimits.MaxTextureSlots, 2u);

        OutPlan.reserve(lCount);
        for (Uint32 i = 0; i < lCount; ++i) { OutPlan.emplace_back(i, 0u, 0u, 0u, 0u); }

        std::stable_sort(OutPlan.begin(), OutPlan.end(),
            [&InKeys](const QuadPlacement& InA, const QuadPlacement& InB)
            { return InKeys[InA.QuadIndex] < InKeys[InB.QuadIndex]; });

        TDynArray<Uint32> lSlotTexIds;   // index i = slot i + 1; slot 0 is white
        lSlotTexIds.reserve(lMaxSlots);

        Uint32 lBatch        = 0;
        Uint32 lQuadsInBatch = 0;

        const bool lHasMasks   = !InMaskIds.empty();
        const bool lHasNormals = !InNormalIds.empty();

        for (QuadPlacement& lPlacement : OutPlan)
        {
            const Uint32 lTexId    = InTextureIds[lPlacement.QuadIndex];
            const Uint32 lMaskId   = (lHasMasks && lPlacement.QuadIndex < InMaskIds.size())
                                         ? InMaskIds[lPlacement.QuadIndex] : 0u;
            const Uint32 lNormalId = (lHasNormals && lPlacement.QuadIndex < InNormalIds.size())
                                         ? InNormalIds[lPlacement.QuadIndex] : 0u;

            Uint32 lSlot     = FindSlot(lSlotTexIds, lTexId);
            Uint32 lMaskSlot = (lMaskId != 0) ? FindSlot(lSlotTexIds, lMaskId) : 0u;

            // New slots this quad needs. A texture used twice by the quad shares its slot.
            Uint32 lNeeded = (lTexId != 0 && lSlot == 0) ? 1u : 0u;
            if (lMaskId != 0 && lMaskSlot == 0 && lMaskId != lTexId) { ++lNeeded; }
            if (lNormalId != 0 && FindSlot(lSlotTexIds, lNormalId) == 0 && lNormalId != lTexId && lNormalId != lMaskId)
            {
                ++lNeeded;
            }

            // Quads first, then samplers: a full batch closes.
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
                // After binding the texture: when the ids match, the mask uses that slot.
                lMaskSlot = FindSlot(lSlotTexIds, lMaskId);

                if (lMaskSlot == 0)
                {
                    lSlotTexIds.emplace_back(lMaskId);
                    lMaskSlot = static_cast<Uint32>(lSlotTexIds.size());
                }
            }

            Uint32 lNormalSlot = 0;
            if (lNormalId != 0)
            {
                // After the texture and the mask: when the ids match, the normal map uses their slot.
                lNormalSlot = FindSlot(lSlotTexIds, lNormalId);

                if (lNormalSlot == 0)
                {
                    lSlotTexIds.emplace_back(lNormalId);
                    lNormalSlot = static_cast<Uint32>(lSlotTexIds.size());
                }
            }

            lPlacement.Batch      = lBatch;
            lPlacement.Slot       = lSlot;
            lPlacement.MaskSlot   = lMaskSlot;
            lPlacement.NormalSlot = lNormalSlot;
            ++lQuadsInBatch;
        }
    }
}
