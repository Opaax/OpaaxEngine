// Suite: rect -> quads geometry (UI/UISlice.h): 9-slice and the fill clip. Pure (a 9-slice only
// needs the texture size).
#include "Core/Maths/Maths.h"

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
            if (Maths::Abs(lMin.x - InMin.x) < 0.01f && Maths::Abs(lMin.y - InMin.y) < 0.01f)
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

TEST_CASE("UISlice: at a scale, a corner is border * scale units wide and still shows border pixels")
{
    // Bhop's case: a 256 px frame with a 112 px border on a 420x90 button, at 0.4.
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 420.f, 90.f });
    const UIMargin lBorder{ 112.f, 112.f, 85.f, 112.f };

    TDynArray<UIQuad> lQuads;
    BuildSlicedQuads(lRect, lBorder, 0.4f, { 256.f, 256.f }, lQuads);
    REQUIRE(lQuads.size() == 9u);

    const UIQuad* lBottomLeft = QuadAt(lQuads, { 0.f, 0.f });
    REQUIRE(lBottomLeft != nullptr);
    CheckVec(lBottomLeft->Bounds.Size(), { 44.8f, 34.f });
    CheckVec(lBottomLeft->UVMax, { 112.f / 256.f, 85.f / 256.f });

    const UIQuad* lTopRight = QuadAt(lQuads, { 420.f - 44.8f, 90.f - 44.8f });
    REQUIRE(lTopRight != nullptr);
    CheckVec(lTopRight->Bounds.Size(), { 44.8f, 44.8f });
    CheckVec(lTopRight->UVMin, { 1.f - 112.f / 256.f, 1.f - 112.f / 256.f });

    // The same border at 2: twice the size on screen, the same pixels.
    TDynArray<UIQuad> lBig;
    BuildSlicedQuads(Bounds2D::FromMinMax({ 0.f, 0.f }, { 1000.f, 1000.f }), lBorder, 2.f, { 256.f, 256.f }, lBig);
    const UIQuad* lBigCorner = QuadAt(lBig, { 0.f, 0.f });
    REQUIRE(lBigCorner != nullptr);
    CheckVec(lBigCorner->Bounds.Size(), { 224.f, 170.f });
    CheckVec(lBigCorner->UVMax, { 112.f / 256.f, 85.f / 256.f });
}

TEST_CASE("UISlice: a rect smaller than the SCALED borders shrinks them; one they fit is left alone")
{
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 50.f, 200.f });

    // 40 + 40 at 1 is 80 units in 50: fitted to 25 each.
    TDynArray<UIQuad> lAtOne;
    BuildSlicedQuads(lRect, Uniform(40.f), 1.f, { 128.f, 128.f }, lAtOne);
    const UIQuad* lShrunk = QuadAt(lAtOne, { 0.f, 0.f });
    REQUIRE(lShrunk != nullptr);
    CHECK(lShrunk->Bounds.Size().x == doctest::Approx(25.f));

    // 40 + 40 at 0.5 is 40 units in 50: kept at 20 each, with a 10 unit middle.
    TDynArray<UIQuad> lAtHalf;
    BuildSlicedQuads(lRect, Uniform(40.f), 0.5f, { 128.f, 128.f }, lAtHalf);
    REQUIRE(lAtHalf.size() == 9u);
    const UIQuad* lKept = QuadAt(lAtHalf, { 0.f, 0.f });
    REQUIRE(lKept != nullptr);
    CHECK(lKept->Bounds.Size().x == doctest::Approx(20.f));
    CHECK(lKept->UVMax.x == doctest::Approx(40.f / 128.f));
}

TEST_CASE("UISlice: a scale of zero or less reads as 1, as does the call without one")
{
    const Bounds2D lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, { 400.f, 200.f });

    for (const float lScale : { 0.f, -3.f, 1.f })
    {
        CAPTURE(lScale);
        TDynArray<UIQuad> lQuads;
        BuildSlicedQuads(lRect, Uniform(16.f), lScale, { 64.f, 64.f }, lQuads);
        const UIQuad* lCorner = QuadAt(lQuads, { 0.f, 0.f });
        REQUIRE(lCorner != nullptr);
        CheckVec(lCorner->Bounds.Size(), { 16.f, 16.f });
    }

    CHECK(SanitizeBorderScale(0.4f) == doctest::Approx(0.4f));
    CHECK(SanitizeBorderScale(0.f) == doctest::Approx(1.f));
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

TEST_CASE("UISlice: MapQuadUVsInto lands every sliced UV inside the frame's rect, and is the identity for a whole texture")
{
    const Bounds2D lRect = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 300.f, 120.f });

    TDynArray<UIQuad> lQuads;
    BuildSlicedQuads(lRect, UIMargin{ 16.f, 16.f, 16.f, 16.f }, { 64.f, 64.f }, lQuads);
    REQUIRE(lQuads.size() == 9u);

    // A 64x64 frame at (128, 64) of a 512x256 sheet, as MakeFrameUV would hand it over.
    const Vector2F lFrameMin{ 0.25f, 0.5f };
    const Vector2F lFrameMax{ 0.375f, 0.75f };
    MapQuadUVsInto(lQuads, lFrameMin, lFrameMax);

    for (const UIQuad& lQuad : lQuads)
    {
        CHECK(lQuad.UVMin.x >= lFrameMin.x - 1e-5f);
        CHECK(lQuad.UVMin.y >= lFrameMin.y - 1e-5f);
        CHECK(lQuad.UVMax.x <= lFrameMax.x + 1e-5f);
        CHECK(lQuad.UVMax.y <= lFrameMax.y + 1e-5f);
    }

    // The corners still sample the border: the first quad's UV span is 16/64 of the frame's span.
    CHECK(lQuads[0].UVMax.x - lQuads[0].UVMin.x == doctest::Approx(0.125f * 0.25f));

    TDynArray<UIQuad> lUnmapped;
    TDynArray<UIQuad> lWhole;
    BuildSlicedQuads(lRect, UIMargin{ 16.f, 16.f, 16.f, 16.f }, { 64.f, 64.f }, lUnmapped);
    BuildSlicedQuads(lRect, UIMargin{ 16.f, 16.f, 16.f, 16.f }, { 64.f, 64.f }, lWhole);
    MapQuadUVsInto(lWhole, { 0.f, 0.f }, { 1.f, 1.f });
    REQUIRE(lWhole.size() == lUnmapped.size());
    for (Uint64 lIndex = 0; lIndex < lWhole.size(); ++lIndex)
    {
        CHECK(lWhole[lIndex].UVMin.x == doctest::Approx(lUnmapped[lIndex].UVMin.x));
        CHECK(lWhole[lIndex].UVMax.y == doctest::Approx(lUnmapped[lIndex].UVMax.y));
    }
}
