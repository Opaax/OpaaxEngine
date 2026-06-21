// Suite: Renderer2D draw-order sort-key packing (Renderer/Renderer2DSortKey.h).
//
// MakeSortKey is header-inline + constexpr, so this suite compiles the function
// itself — no DLL symbol needed. It pins the bit layout the batch sort relies on:
//   [Layer : bits 32..39][biased OrderInLayer : bits 8..23][texSlot : bits 0..7]
#include <doctest.h>

#include "Renderer/Renderer2DSortKey.h"

#include <algorithm>
#include <vector>

using namespace Opaax;

TEST_CASE("MakeSortKey: texture slot occupies the low 8 bits")
{
    CHECK((MakeSortKey(ERenderLayer::Default, 0, 5u) & 0xFFu) == 5u);
    // slot is masked to 8 bits — anything above 0xFF wraps into the byte
    CHECK((MakeSortKey(ERenderLayer::Default, 0, 0x1FFu) & 0xFFu) == 0xFFu);
}

TEST_CASE("MakeSortKey: OrderInLayer is biased by 32768 into bits 8..23")
{
    auto OrderField = [](Uint64 InKey) { return (InKey >> 8) & 0xFFFFu; };

    CHECK(OrderField(MakeSortKey(ERenderLayer::Default, -32768, 0u)) == 0u);
    CHECK(OrderField(MakeSortKey(ERenderLayer::Default, 0, 0u))      == 32768u);
    CHECK(OrderField(MakeSortKey(ERenderLayer::Default, 32767, 0u))  == 65535u);
}

TEST_CASE("MakeSortKey: layer occupies bits 32..39")
{
    auto LayerField = [](Uint64 InKey) { return (InKey >> 32) & 0xFFu; };

    CHECK(LayerField(MakeSortKey(ERenderLayer::Background, 0, 0u)) == 0u);
    CHECK(LayerField(MakeSortKey(ERenderLayer::UI,         0, 0u)) == 3u);
}

TEST_CASE("MakeSortKey: negative orders sort before positive within a layer")
{
    const Uint64 lNeg  = MakeSortKey(ERenderLayer::Default, -100, 0u);
    const Uint64 lZero = MakeSortKey(ERenderLayer::Default,    0, 0u);
    const Uint64 lPos  = MakeSortKey(ERenderLayer::Default,  100, 0u);

    CHECK(lNeg  < lZero);
    CHECK(lZero < lPos);
}

TEST_CASE("MakeSortKey: layer dominates order and slot")
{
    // Painter's algorithm: a higher band always draws on top. A lower layer with the
    // maximum order + slot must still sort BEFORE a higher layer at its minimum.
    const Uint64 lLowLayerMax  = MakeSortKey(ERenderLayer::Background, 32767, 0xFFu);
    const Uint64 lHighLayerMin = MakeSortKey(ERenderLayer::Default,   -32768, 0u);

    CHECK(lLowLayerMax < lHighLayerMin);
}

TEST_CASE("MakeSortKey: matches the documented bit layout exactly")
{
    const Uint64 lKey      = MakeSortKey(ERenderLayer::Foreground, 7, 3u);
    const Uint64 lExpected = (static_cast<Uint64>(2) << 32)              // Foreground = 2
                           | (static_cast<Uint64>(7 + 32768) << 8)
                           |  static_cast<Uint64>(3);
    CHECK(lKey == lExpected);
}

// The key must be usable in a constant expression (compile-time sortability is part
// of the contract the constexpr hoist buys us).
static_assert(MakeSortKey(ERenderLayer::Default, 0, 0u)
                  == ((static_cast<Uint64>(1) << 32) | (static_cast<Uint64>(32768) << 8)),
              "MakeSortKey must be a constant expression");

// The frame-global emit (Renderer2D::EmitFrame) std::stable_sorts command indices by sort key. This
// pins the property that makes the reshape correct: equal keys (same Layer + OrderInLayer, slot field
// 0) keep SUBMISSION order, so same-band overlapping sprites draw in the order they were issued.
TEST_CASE("Frame-global stable sort: equal keys keep submission order")
{
    struct Cmd { Uint64 Key; int Submission; };
    const std::vector<Cmd> lCmds = {
        { MakeSortKey(ERenderLayer::Default,    5, 0u), 0 },
        { MakeSortKey(ERenderLayer::Background, 0, 0u), 1 },  // lowest layer -> sorts first
        { MakeSortKey(ERenderLayer::Default,    5, 0u), 2 },  // equal key to submission 0
        { MakeSortKey(ERenderLayer::Default,    5, 0u), 3 },  // equal key to submission 0
        { MakeSortKey(ERenderLayer::UI,         0, 0u), 4 },  // highest layer -> sorts last
    };

    std::vector<Uint32> lIdx(lCmds.size());
    for (Uint32 i = 0; i < lIdx.size(); ++i) { lIdx[i] = i; }

    std::stable_sort(lIdx.begin(), lIdx.end(),
        [&](Uint32 InA, Uint32 InB) { return lCmds[InA].Key < lCmds[InB].Key; });

    CHECK(lCmds[lIdx[0]].Submission == 1);  // Background
    CHECK(lCmds[lIdx[1]].Submission == 0);  // first Default,5 (submission order preserved)
    CHECK(lCmds[lIdx[2]].Submission == 2);
    CHECK(lCmds[lIdx[3]].Submission == 3);
    CHECK(lCmds[lIdx[4]].Submission == 4);  // UI
}
