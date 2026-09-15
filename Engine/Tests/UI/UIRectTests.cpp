// Suite: the UI rect model (UI/UIRect.h) — Unity's RectTransform resolution, Y-up.
//
// Anchors are what let a HUD survive a 21:9 screen, so the cases are the three shapes a HUD is
// made of: a full stretch, a corner-pinned box, and the default centred box.
#include <doctest.h>

#include "UI/UIRect.h"

using namespace Opaax;

namespace
{
    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x));
        CHECK(InA.y == doctest::Approx(InB.y));
    }
}

TEST_CASE("UIRect: stretched anchors with no delta ARE the parent")
{
    const Bounds2D lParent = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 1920.f, 1080.f });

    UIRect lRect;
    lRect.AnchorMin = { 0.f, 0.f };
    lRect.AnchorMax = { 1.f, 1.f };
    lRect.SizeDelta = { 0.f, 0.f };

    const Bounds2D lOut = ResolveRect(lRect, lParent);
    CheckVec(lOut.Min(), lParent.Min());
    CheckVec(lOut.Max(), lParent.Max());
}

TEST_CASE("UIRect: a corner-anchored box sits inset from that corner")
{
    const Bounds2D lParent = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 1920.f, 1080.f });

    UIRect lRect;
    lRect.AnchorMin        = { 1.f, 1.f };
    lRect.AnchorMax        = { 1.f, 1.f };
    lRect.Pivot            = { 1.f, 1.f };
    lRect.AnchoredPosition = { -10.f, -10.f };
    lRect.SizeDelta        = { 100.f, 50.f };

    const Bounds2D lOut = ResolveRect(lRect, lParent);
    CheckVec(lOut.Max(),  { 960.f - 10.f, 540.f - 10.f });
    CheckVec(lOut.Size(), { 100.f, 50.f });
}

TEST_CASE("UIRect: the default is a 100x100 box centred on its parent")
{
    const Bounds2D lParent = Bounds2D::FromCenterSize({ 200.f, -50.f }, { 800.f, 600.f });

    const Bounds2D lOut = ResolveRect(UIRect{}, lParent);
    CheckVec(lOut.Center, lParent.Center);
    CheckVec(lOut.Size(), { 100.f, 100.f });
}

TEST_CASE("UIRect: a stretched axis grows with the parent, a point axis does not")
{
    UIRect lRect;
    lRect.AnchorMin = { 0.f, 0.5f };
    lRect.AnchorMax = { 1.f, 0.5f };
    lRect.SizeDelta = { -40.f, 30.f };   // 20 in from each side; a fixed 30 tall

    const Bounds2D lNarrow = ResolveRect(lRect, Bounds2D::FromCenterSize({ 0.f, 0.f }, { 1000.f, 500.f }));
    const Bounds2D lWide   = ResolveRect(lRect, Bounds2D::FromCenterSize({ 0.f, 0.f }, { 2000.f, 500.f }));

    CheckVec(lNarrow.Size(), { 960.f, 30.f });
    CheckVec(lWide.Size(),   { 1960.f, 30.f });
}

TEST_CASE("UIRect: an INVERTED anchor pair is clamped, not honoured (UI19)")
{
    const Bounds2D lParent = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 1920.f, 1080.f });

    // Exactly what their Hud.opaaxui held: Min above Max on both axes. Un-clamped this makes a
    // negative anchor size, a negative widget size, and FromMinMax silently sorts the corners —
    // so the widget resolved to a ~1788x965 box centred on the origin instead of a 132x115 one.
    UIRect lInverted;
    lInverted.AnchorMin        = { 1.f, 1.f };
    lInverted.AnchorMax        = { 0.f, 0.f };
    lInverted.AnchoredPosition = { 1.f, 1.f };
    lInverted.SizeDelta        = { 132.f, 115.f };

    const Bounds2D lOut = ResolveRect(lInverted, lParent);

    // Clamped to a POINT anchor at (1,1), so it is the size it says it is, at the parent's corner.
    CheckVec(lOut.Size(), { 132.f, 115.f });

    UIRect lPoint = lInverted;
    lPoint.AnchorMax = { 1.f, 1.f };   // what the clamp makes of it
    const Bounds2D lExpected = ResolveRect(lPoint, lParent);
    CheckVec(lOut.Center, lExpected.Center);
}

TEST_CASE("UIRect: a normal pair is untouched by the clamp")
{
    const Bounds2D lParent = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 1000.f, 500.f });

    UIRect lStretch;
    lStretch.AnchorMin = { 0.25f, 0.f };
    lStretch.AnchorMax = { 0.75f, 1.f };
    lStretch.SizeDelta = { 0.f, 0.f };

    const Bounds2D lOut = ResolveRect(lStretch, lParent);
    CheckVec(lOut.Size(), { 500.f, 500.f });   // half the width, the full height — as authored
}
