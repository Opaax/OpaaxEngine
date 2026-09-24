// Suite: Renderer2D draw-order sort-key packing (Renderer/Renderer2DSortKey.h).
//
// MakeSortKey is header-inline + constexpr, so this suite compiles the function
// itself — no DLL symbol needed. It pins the bit layout the batch sort relies on:
//   [Layer : bits 32..39][biased OrderInLayer : bits 8..23][texSlot : bits 0..7]
#include <doctest.h>

#include "Renderer/Renderer2DSortKey.h"

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

    // Assert the FIELD against the band's own value, never against a hardcoded ordinal: adding a
    // layer to RenderLayerList.h must not break a test about bit POSITIONS. (It did once — inserting
    // Debug shifted UI from 3 to 4.)
    CHECK(LayerField(MakeSortKey(ERenderLayer::Background, 0, 0u)) == static_cast<Uint64>(ERenderLayer::Background));
    CHECK(LayerField(MakeSortKey(ERenderLayer::UI,         0, 0u)) == static_cast<Uint64>(ERenderLayer::UI));

    // The topmost band still fits the 8-bit field — nothing bleeds past bit 39 as the list grows.
    constexpr Uint8 lTopBand = static_cast<Uint8>(ERenderLayer::Count) - 1;
    CHECK(LayerField(MakeSortKey(static_cast<ERenderLayer>(lTopBand), 0, 0u)) == lTopBand);
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
    // Expected is recomputed from the documented shifts, independently of MakeSortKey's body —
    // that is the test's value. The band comes from the enum (see above) so growing the layer
    // list cannot invalidate a packing assertion.
    const Uint64 lKey      = MakeSortKey(ERenderLayer::Foreground, 7, 3u);
    const Uint64 lExpected = (static_cast<Uint64>(ERenderLayer::Foreground) << 32)
                           | (static_cast<Uint64>(7 + 32768) << 8)
                           |  static_cast<Uint64>(3);
    CHECK(lKey == lExpected);
}

// The key must be usable in a constant expression (compile-time sortability is part
// of the contract the constexpr hoist buys us).
static_assert(MakeSortKey(ERenderLayer::Default, 0, 0u)
                  == ((static_cast<Uint64>(ERenderLayer::Default) << 32) | (static_cast<Uint64>(32768) << 8)),
              "MakeSortKey must be a constant expression");
