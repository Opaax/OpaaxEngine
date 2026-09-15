// Suite: the rect → quads geometry (UI/UISlice.h) — 9-slice and the fill clip, U5b.
//
// WHY IT IS PURE: a 9-slice needs a texture's SIZE, not a texture, so the whole of it is gated
// here with no GL context, no provider and no canvas — ResolveRect's precedent one level down.
// What a smoke run can see is the picture; what it cannot see is a corner that stretched by
// half a pixel, or a border silently eating a rect narrower than itself.
#include <cmath>

#include <doctest.h>

#include "UI/UISlice.h"

using namespace Opaax;

namespace
{
    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x));
        CHECK(InA.y == doctest::Approx(InB.y));
    }

    UIMargin Uniform(const float InAll) { return UIMargin{ InAll, InAll, InAll, InAll }; }

    /** The quad whose min corner is InMin, or nothing — the emission ORDER is not the contract. */
    const UIQuad* QuadAt(const TDynArray<UIQuad>& InQuads, const Vector2F& InMin)
    {
        for (const UIQuad& lQuad : InQuads)
        {
            const Vector2F lMin = lQuad.Bounds.Min();
            if (std::fabs(lMin.x - InMin.x) < 0.01f && std::fabs(lMin.y - InMin.y) < 0.01f)
            {
                return &lQuad;
            }
        }

        return nullptr;
    }
}

TEST_CASE("UISlice: a 16px border on a 64px texture makes 9 quads, and the CORNERS keep their size")
{
    // 400x200 rect, a quarter-of-the-texture border: the corners must come out 16x16 whatever the
    // rect does, which is the entire point of slicing.
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 400.f, 200.f });

    TDynArray<UIQuad> lQuads;
    BuildSlicedQuads(lRect, Uniform(16.f), { 64.f, 64.f }, lQuads);

    REQUIRE(lQuads.size() == 9);

    const UIQuad* lBottomLeft = QuadAt(lQuads, { 0.f, 0.f });
    REQUIRE(lBottomLeft != nullptr);
    CheckVec(lBottomLeft->Bounds.Size(), { 16.f, 16.f });
    CheckVec(lBottomLeft->UVMin, { 0.f, 0.f });
    CheckVec(lBottomLeft->UVMax, { 0.25f, 0.25f });

    const UIQuad* lTopRight = QuadAt(lQuads, { 384.f, 184.f });
    REQUIRE(lTopRight != nullptr);
    CheckVec(lTopRight->Bounds.Size(), { 16.f, 16.f });
    CheckVec(lTopRight->UVMin, { 0.75f, 0.75f });
    CheckVec(lTopRight->UVMax, { 1.f, 1.f });

    // The centre takes everything the border left, and samples the middle of the texture.
    const UIQuad* lCentre = QuadAt(lQuads, { 16.f, 16.f });
    REQUIRE(lCentre != nullptr);
    CheckVec(lCentre->Bounds.Size(), { 368.f, 168.f });
    CheckVec(lCentre->UVMin, { 0.25f, 0.25f });
    CheckVec(lCentre->UVMax, { 0.75f, 0.75f });
}

TEST_CASE("UISlice: a border on ONE axis emits 3 quads — a degenerate row is skipped, not empty")
{
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 400.f, 200.f });

    UIMargin lSides;
    lSides.Left  = 16.f;
    lSides.Right = 16.f;

    TDynArray<UIQuad> lQuads;
    BuildSlicedQuads(lRect, lSides, { 64.f, 64.f }, lQuads);

    CHECK(lQuads.size() == 3);

    for (const UIQuad& lQuad : lQuads)
    {
        CHECK(lQuad.Bounds.Size().y == doctest::Approx(200.f));   // one full-height row
        CHECK(lQuad.UVMin.y == doctest::Approx(0.f));
        CHECK(lQuad.UVMax.y == doctest::Approx(1.f));
    }
}

TEST_CASE("UISlice: a rect NARROWER than its own borders shrinks them, and the UVs do not follow")
{
    // 20 wide with a 16+16 border: the corners would overlap and the middle would invert. They are
    // fitted to 10+10 instead — and the UVs keep the authored quarter, so the art compresses.
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 20.f, 200.f });

    TDynArray<UIQuad> lQuads;
    BuildSlicedQuads(lRect, Uniform(16.f), { 64.f, 64.f }, lQuads);

    // No middle column survives (its width is exactly 0), so 3 rows x 2 columns.
    REQUIRE(lQuads.size() == 6);

    const UIQuad* lBottomLeft = QuadAt(lQuads, { 0.f, 0.f });
    REQUIRE(lBottomLeft != nullptr);
    CHECK(lBottomLeft->Bounds.Size().x == doctest::Approx(10.f));
    CHECK(lBottomLeft->UVMax.x == doctest::Approx(0.25f));

    for (const UIQuad& lQuad : lQuads)
    {
        CHECK(lQuad.Bounds.Size().x > 0.f);
        CHECK(lQuad.Bounds.Size().y > 0.f);
    }
}

TEST_CASE("UISlice: a texture with no size degrades to ONE quad rather than dividing by it")
{
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 400.f, 200.f });

    TDynArray<UIQuad> lQuads;
    BuildSlicedQuads(lRect, Uniform(16.f), { 0.f, 0.f }, lQuads);

    REQUIRE(lQuads.size() == 1);
    CheckVec(lQuads[0].Bounds.Size(), { 400.f, 200.f });
    CheckVec(lQuads[0].UVMax, { 1.f, 1.f });
}

TEST_CASE("UISlice: a clip cuts the bounds AND the UVs by the same fraction")
{
    TDynArray<UIQuad> lQuads;
    UIQuad&           lQuad = lQuads.emplace_back();
    lQuad.Bounds = Bounds2D::FromMinMax({ -100.f, -20.f }, { 100.f, 20.f });

    ClipQuadsTo(lQuads, Bounds2D::FromMinMax({ -100.f, -20.f }, { 0.f, 20.f }));

    REQUIRE(lQuads.size() == 1);
    CheckVec(lQuads[0].Bounds.Min(), { -100.f, -20.f });
    CheckVec(lQuads[0].Bounds.Size(), { 100.f, 40.f });
    CheckVec(lQuads[0].UVMin, { 0.f, 0.f });
    CheckVec(lQuads[0].UVMax, { 0.5f, 1.f });
}

TEST_CASE("UISlice: a clip keeps what is inside, cuts what straddles and DROPS what is outside")
{
    // The three cases a filled 9-slice produces at once: the left cap survives whole, the middle is
    // cut, the right cap is gone. This is why fill and slice compose instead of excluding.
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 300.f, 100.f });

    TDynArray<UIQuad> lQuads;
    BuildSlicedQuads(lRect, UIMargin{ 30.f, 30.f, 0.f, 0.f }, { 90.f, 100.f }, lQuads);
    REQUIRE(lQuads.size() == 3);

    ClipQuadsTo(lQuads, Bounds2D::FromMinMax({ 0.f, 0.f }, { 150.f, 100.f }));

    REQUIRE(lQuads.size() == 2);

    // The left cap is untouched: a clip that does not reach a quad changes nothing about it.
    const UIQuad* lCap = QuadAt(lQuads, { 0.f, 0.f });
    REQUIRE(lCap != nullptr);
    CHECK(lCap->Bounds.Size().x == doctest::Approx(30.f));
    CHECK(lCap->UVMax.x == doctest::Approx(1.f / 3.f));

    // The middle ran 30..270 and now ends at 150 — half of it, and half of its UV span.
    const UIQuad* lMiddle = QuadAt(lQuads, { 30.f, 0.f });
    REQUIRE(lMiddle != nullptr);
    CHECK(lMiddle->Bounds.Size().x == doctest::Approx(120.f));
    CHECK(lMiddle->UVMin.x == doctest::Approx(1.f / 3.f));
    CHECK(lMiddle->UVMax.x == doctest::Approx(1.f / 3.f + (1.f / 3.f) * 0.5f));
}
