// Suite: OpaaxStringID (Core/String/OpaaxStringID.hpp) — the interned-string handle.
//
// On the DLL-safety invariant (I2) — what these tests do and do NOT prove, stated honestly:
//
// "One intern pool per process" is guaranteed STRUCTURALLY, at link time, not by anything below:
// GetPool() has exactly one definition, in the engine DLL's .cpp, exported via OPAAX_API. A consumer
// has no definition to duplicate. No runtime assertion from a single module can distinguish "one
// pool" from "two pools that happen to agree", so do not read these cases as proving that.
//
// What they DO pin: the interning contract, and — because OpaaxTests.exe is a different module from
// OpaaxEngine.dll — a PARTIAL regression, i.e. someone re-inlining one entry point but not the other.
// The growth cases below have the ctor as writer and PoolSize as reader; if those two ever end up in
// different modules' pools, the observed growth goes to 0 and they fail. That is the realistic
// regression (a well-meaning "just make this inline, it's a one-liner"), and it is worth a guard.
#include <doctest.h>

#include "Core/String/OpaaxStringID.hpp"
#include "Renderer/RenderLayer.h"

using namespace Opaax;

TEST_CASE("OpaaxStringID: default-constructed is the invalid 'None' id")
{
    const OpaaxStringID lId;

    CHECK_FALSE(lId.IsValid());
    CHECK(lId.GetId() == 0u);
    CHECK(lId.ToString() == OpaaxString("None"));
}

TEST_CASE("OpaaxStringID: an empty string interns to None, not to a new entry")
{
    const OpaaxStringID lId(OpaaxString(""));

    CHECK_FALSE(lId.IsValid());
    CHECK(lId.GetId() == 0u);
}

TEST_CASE("OpaaxStringID: the same text always yields the same id (that is the whole point)")
{
    const OpaaxStringID lFirst  = OPAAX_ID("OpaaxStringIDTests_Repeat");
    const OpaaxStringID lSecond = OPAAX_ID("OpaaxStringIDTests_Repeat");

    CHECK(lFirst == lSecond);
    CHECK(lFirst.GetId() == lSecond.GetId());
    CHECK(lFirst.IsValid());
}

TEST_CASE("OpaaxStringID: different text yields different ids, and round-trips back to the text")
{
    const OpaaxStringID lA = OPAAX_ID("OpaaxStringIDTests_Alpha");
    const OpaaxStringID lB = OPAAX_ID("OpaaxStringIDTests_Beta");

    CHECK(lA != lB);
    CHECK(lA.ToString() == OpaaxString("OpaaxStringIDTests_Alpha"));
    CHECK(lB.ToString() == OpaaxString("OpaaxStringIDTests_Beta"));
}

TEST_CASE("OpaaxStringID: one pool across the DLL boundary — a NEW string grows it by exactly 1")
{
    // The discriminating assertion (see the file header): the ctor writes and PoolSize reads. If a
    // consuming module owned a second pool, these two would be looking at different tables and the
    // observed growth would be 0, not 1.
    const Uint32 lBefore = OpaaxStringID::PoolSize();

    const OpaaxStringID lFresh = OPAAX_ID("OpaaxStringIDTests_UniqueGrowthProbe");

    const Uint32 lAfter = OpaaxStringID::PoolSize();

    CHECK(lFresh.IsValid());
    CHECK(lAfter == lBefore + 1u);
}

TEST_CASE("OpaaxStringID: one pool across the DLL boundary — a REPEAT does not grow it")
{
    // Intern once so the entry exists regardless of case order, then measure the repeat.
    const OpaaxStringID lFirst = OPAAX_ID("OpaaxStringIDTests_NoGrowthProbe");

    const Uint32        lBefore = OpaaxStringID::PoolSize();
    const OpaaxStringID lAgain  = OPAAX_ID("OpaaxStringIDTests_NoGrowthProbe");
    const Uint32        lAfter  = OpaaxStringID::PoolSize();

    CHECK(lAgain == lFirst);
    CHECK(lAfter == lBefore);
}

TEST_CASE("OpaaxStringID: ids interned by engine headers agree with ids interned here")
{
    // g_RenderLayerIDs is built from the same intern table; naming a layer from this module must
    // land on the identical handle. A cheap guard that the canonical-name LUT and ad-hoc call sites
    // cannot drift apart.
    CHECK(OPAAX_ID("Background") == g_RenderLayerIDs[static_cast<Uint8>(ERenderLayer::Background)]);
    CHECK(OPAAX_ID("Debug")      == g_RenderLayerIDs[static_cast<Uint8>(ERenderLayer::Debug)]);
    CHECK(RenderLayerFromStringID(OPAAX_ID("Debug")) == ERenderLayer::Debug);
}
