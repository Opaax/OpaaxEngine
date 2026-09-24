// Suite: UISafeArea (UI/Widgets/UISafeArea.h) — U5b.
//
// What is gated: that the inset rect is the one CHILDREN anchor to and the one a hit-test asks,
// which is the whole reason it overrides its bounds rather than carrying a second rect. The
// numbers are percentages of a known canvas, so "does it adapt to 21:9" is an assertion here
// rather than something only a resized window could answer.
#include <doctest.h>

#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UISafeArea.h"

using namespace Opaax;

namespace
{
    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x));
        CHECK(InA.y == doctest::Approx(InB.y));
    }

    /** Stretched over the whole parent — what a background or a HUD layer is anchored as. */
    UIRect Stretch()
    {
        UIRect lRect;
        lRect.AnchorMin = { 0.f, 0.f };
        lRect.AnchorMax = { 1.f, 1.f };
        lRect.SizeDelta = { 0.f, 0.f };
        return lRect;
    }
}

TEST_CASE("UISafeArea: it starts stretched, and 5% per edge leaves the title-safe 90%")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    auto* lSafe = static_cast<UISafeArea*>(lCanvas.Root().AddChild(MakeUnique<UISafeArea>()));
    lCanvas.Update();

    // The default rect is the parent's, not a 100x100 box — the one widget that could not sensibly
    // start smaller than what it measures.
    CheckVec(lSafe->GetBounds().Size(), { 1920.f * 0.9f, 1080.f * 0.9f });
    CheckVec(lSafe->GetBounds().Center, { 0.f, 0.f });
}

TEST_CASE("UISafeArea: a stretched CHILD lands inside the insets, not inside the raw rect")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIWidget* lSafe  = lCanvas.Root().AddChild(MakeUnique<UISafeArea>());
    UIWidget* lChild = lSafe->AddChild(MakeUnique<UIImage>());
    lChild->SetRect(Stretch());
    lCanvas.Update();

    CheckVec(lChild->GetBounds().Size(), { 1920.f * 0.9f, 1080.f * 0.9f });
    CheckVec(lChild->GetBounds().Max(), { 1920.f * 0.45f, 1080.f * 0.45f });

    // And the hit-test agrees: a point in the unsafe band misses the child that used to cover it.
    CHECK(lCanvas.HitTest({ 950.f, 0.f }) == nullptr);
    CHECK(lCanvas.HitTest({ 800.f, 0.f }) == lChild);
}

TEST_CASE("UISafeArea: the insets are FRACTIONS, so a wider target keeps the same percentage")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    auto* lSafe = static_cast<UISafeArea*>(lCanvas.Root().AddChild(MakeUnique<UISafeArea>()));
    lCanvas.Update();

    // 21:9 — the case their "adapting to all screen even wide" names. An ABSOLUTE inset would be a
    // shrinking share of a widening screen; a fraction is the same band it was.
    lCanvas.SetTargetSize(2560, 1080);
    lCanvas.Update();

    const float lWide = 1080.f * (2560.f / 1080.f);
    CheckVec(lSafe->GetBounds().Size(), { lWide * 0.9f, 1080.f * 0.9f });
}

TEST_CASE("UISafeArea: per-edge insets, and changing them re-lays the subtree")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    auto*     lSafe  = static_cast<UISafeArea*>(lCanvas.Root().AddChild(MakeUnique<UISafeArea>()));
    UIWidget* lChild = lSafe->AddChild(MakeUnique<UIImage>());
    lChild->SetRect(Stretch());
    lCanvas.Update();

    UIMargin lInsets;
    lInsets.Left = 0.25f;
    lInsets.Top  = 0.5f;
    lSafe->SetInsets(lInsets);

    const UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts == 2);   // the safe area and the child under it

    // Left in by a quarter, top in by half, right and bottom untouched.
    CheckVec(lSafe->GetBounds().Min(), { -960.f + 1920.f * 0.25f, -540.f });
    CheckVec(lSafe->GetBounds().Max(), { 960.f, -540.f + 1080.f * 0.5f });
    CheckVec(lChild->GetBounds().Size(), lSafe->GetBounds().Size());
}

TEST_CASE("UISafeArea: insets that would swallow the rect are fitted, never inverted")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    auto* lSafe = static_cast<UISafeArea*>(lCanvas.Root().AddChild(MakeUnique<UISafeArea>()));

    // A hand-edited file is not the inspector (**UI19**'s lesson): 0.8 + 0.8 is authorable, and a
    // negative edge is too. Both must leave a rect that still resolves.
    UIMargin lBad;
    lBad.Left   = 0.8f;
    lBad.Right  = 0.8f;
    lBad.Bottom = -1.f;
    lSafe->SetInsets(lBad);
    lCanvas.Update();

    CHECK(lSafe->GetBounds().Size().x == doctest::Approx(1920.f * 0.1f));
    CHECK(lSafe->GetBounds().Size().y > 0.f);
    CHECK(lSafe->GetBounds().Min().y == doctest::Approx(-540.f));   // a negative inset is no inset
}
