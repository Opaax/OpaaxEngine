// Suite: the pass batch plan (Renderer/Renderer2DBatchPlan.h).
//
// This is the ⑥ bug's regression gate. Renderer2D used to sort INSIDE Flush, i.e. after the batch
// had already been cut, so past MAX_QUADS or the sampler limit a later batch drew over an earlier
// layer. Every case below is about the order the plan EMITS in, never about pixels — which is why
// it needs no GL context.
#include <doctest.h>

#include "Renderer/Renderer2DBatchPlan.h"
#include "Renderer/Renderer2DSortKey.h"

using namespace Opaax;

namespace
{
    TDynArray<Uint64> Keys(TInitArray<ERenderLayer> InLayers)
    {
        TDynArray<Uint64> lKeys;
        for (const ERenderLayer lLayer : InLayers) { lKeys.emplace_back(MakeSortKey(lLayer, 0, 0u)); }
        return lKeys;
    }

    // The layer each emitted quad came from, in emit order.
    TDynArray<Uint32> EmittedIndices(const TDynArray<QuadPlacement>& InPlan)
    {
        TDynArray<Uint32> lOut;
        for (const QuadPlacement& lPlacement : InPlan) { lOut.emplace_back(lPlacement.QuadIndex); }
        return lOut;
    }
}

TEST_CASE("PlanQuadBatches: no quads, no plan")
{
    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches({}, {}, QuadBatchLimits{}, lPlan);

    CHECK(lPlan.empty());
}

TEST_CASE("PlanQuadBatches: one batch emits in layer order, not submission order")
{
    const TDynArray<Uint64> lKeys = Keys({ ERenderLayer::UI, ERenderLayer::Background,
                                           ERenderLayer::UI, ERenderLayer::Background });
    const TDynArray<Uint32> lTex(4, 0u);

    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches(lKeys, lTex, QuadBatchLimits{}, lPlan);

    CHECK(EmittedIndices(lPlan) == TDynArray<Uint32>{ 1u, 3u, 0u, 2u });
    for (const QuadPlacement& lPlacement : lPlan) { CHECK(lPlacement.Batch == 0u); }
}

TEST_CASE("PlanQuadBatches: the sort survives the batch cut")
{
    // THE ⑥ REGRESSION GATE. Same input as above with room for two quads per flush: both
    // Background quads must land in batch 0 and both UI quads in batch 1, so the later flush
    // still draws on top. Sorting inside the flush cannot produce this — it would cut the pass
    // as [UI, Background] then [UI, Background] and paint background over UI.
    const TDynArray<Uint64> lKeys = Keys({ ERenderLayer::UI, ERenderLayer::Background,
                                           ERenderLayer::UI, ERenderLayer::Background });
    const TDynArray<Uint32> lTex(4, 0u);

    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches(lKeys, lTex, QuadBatchLimits{ 2u, 16u }, lPlan);

    REQUIRE(lPlan.size() == 4u);
    CHECK(EmittedIndices(lPlan) == TDynArray<Uint32>{ 1u, 3u, 0u, 2u });
    CHECK(lPlan[0].Batch == 0u);
    CHECK(lPlan[1].Batch == 0u);
    CHECK(lPlan[2].Batch == 1u);
    CHECK(lPlan[3].Batch == 1u);
}

TEST_CASE("PlanQuadBatches: equal keys keep submission order")
{
    const TDynArray<Uint64> lKeys(5, MakeSortKey(ERenderLayer::Default, 0, 0u));
    const TDynArray<Uint32> lTex(5, 0u);

    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches(lKeys, lTex, QuadBatchLimits{}, lPlan);

    CHECK(EmittedIndices(lPlan) == TDynArray<Uint32>{ 0u, 1u, 2u, 3u, 4u });
}

TEST_CASE("PlanQuadBatches: a batch closes when the samplers run out")
{
    // Two slots = white + exactly one texture, so three distinct textures need three flushes.
    const TDynArray<Uint64> lKeys(3, MakeSortKey(ERenderLayer::Default, 0, 0u));
    const TDynArray<Uint32> lTex{ 7u, 8u, 9u };

    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches(lKeys, lTex, QuadBatchLimits{ 1000u, 2u }, lPlan);

    REQUIRE(lPlan.size() == 3u);
    for (Uint32 i = 0; i < 3u; ++i)
    {
        CHECK(lPlan[i].Batch == i);
        CHECK(lPlan[i].Slot  == 1u);   // slot 0 stays the white texture in every batch
    }
}

TEST_CASE("PlanQuadBatches: a repeated texture reuses its slot")
{
    const TDynArray<Uint64> lKeys(4, MakeSortKey(ERenderLayer::Default, 0, 0u));
    const TDynArray<Uint32> lTex{ 7u, 8u, 7u, 0u };

    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches(lKeys, lTex, QuadBatchLimits{}, lPlan);

    REQUIRE(lPlan.size() == 4u);
    for (const QuadPlacement& lPlacement : lPlan) { CHECK(lPlacement.Batch == 0u); }

    CHECK(lPlan[0].Slot == 1u);
    CHECK(lPlan[1].Slot == 2u);
    CHECK(lPlan[2].Slot == 1u);   // texture 7 again — no second slot
    CHECK(lPlan[3].Slot == 0u);   // untextured — the white slot, and it costs nothing
}

TEST_CASE("PlanQuadBatches: the default limits hold 15 textures per batch")
{
    // 16 sampler slots, slot 0 permanently white — the count the shader's u_Textures[16] allows.
    TDynArray<Uint64> lKeys;
    TDynArray<Uint32> lTex;
    for (Uint32 i = 0; i < 16u; ++i)
    {
        lKeys.emplace_back(MakeSortKey(ERenderLayer::Default, 0, 0u));
        lTex.emplace_back(i + 1u);
    }

    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches(lKeys, lTex, QuadBatchLimits{}, lPlan);

    REQUIRE(lPlan.size() == 16u);
    CHECK(lPlan[14].Batch == 0u);
    CHECK(lPlan[14].Slot  == 15u);
    CHECK(lPlan[15].Batch == 1u);
    CHECK(lPlan[15].Slot  == 1u);
}

TEST_CASE("PlanQuadBatches: degenerate limits terminate")
{
    // Zeroes are read as "one quad, white plus one texture" — a batch nothing fits in would loop.
    const TDynArray<Uint64> lKeys(3, MakeSortKey(ERenderLayer::Default, 0, 0u));
    const TDynArray<Uint32> lTex{ 1u, 2u, 3u };

    TDynArray<QuadPlacement> lPlan;
    PlanQuadBatches(lKeys, lTex, QuadBatchLimits{ 0u, 0u }, lPlan);

    REQUIRE(lPlan.size() == 3u);
    CHECK(lPlan[0].Batch == 0u);
    CHECK(lPlan[1].Batch == 1u);
    CHECK(lPlan[2].Batch == 2u);
}
