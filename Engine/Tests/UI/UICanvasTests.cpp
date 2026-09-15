// Suite: the canvas walk (UI/UICanvas.h) — the invalidation contract the block was named for.
//
// The numbers are the design: a same-aspect resize lays out NOTHING, an aspect change re-lays
// only what moved, an idle frame costs 0/0. The stats are asserted, not the pictures.
#include <doctest.h>

#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x));
        CHECK(InA.y == doctest::Approx(InB.y));
    }

    UIRect TopRight(const Vector2F& InSize)
    {
        UIRect lRect;
        lRect.AnchorMin = lRect.AnchorMax = lRect.Pivot = { 1.f, 1.f };
        lRect.SizeDelta = InSize;
        return lRect;
    }

    UIRect Centred(const Vector2F& InSize, const Vector2F& InOffset = { 0.f, 0.f })
    {
        UIRect lRect;
        lRect.AnchoredPosition = InOffset;
        lRect.SizeDelta        = InSize;
        return lRect;
    }
}

TEST_CASE("UICanvas: 1080 tall at 16:9 is a 1920x1080 root centred on the origin")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    const UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts == 1);

    CheckVec(lCanvas.Root().GetBounds().Center, { 0.f, 0.f });
    CheckVec(lCanvas.Root().GetBounds().Size(), { 1920.f, 1080.f });
    CheckVec(lCanvas.GetVisibleBounds().Size(), { 1920.f, 1080.f });
}

TEST_CASE("UICanvas: an aspect change moves the corner-anchored widget and re-lays only what moved")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIWidget* lCorner  = lCanvas.Root().AddChild(MakeUnique<UIImage>());
    UIWidget* lCentre  = lCanvas.Root().AddChild(MakeUnique<UIImage>());
    UIWidget* lNested  = lCentre->AddChild(MakeUnique<UIImage>());
    lCorner->SetRect(TopRight({ 100.f, 50.f }));
    lCentre->SetRect(Centred({ 200.f, 200.f }));
    lNested->SetRect(Centred({ 20.f, 20.f }));
    lCanvas.Update();

    const Vector2F lCornerBefore = lCorner->GetBounds().Max();
    const Vector2F lCentreBefore = lCentre->GetBounds().Center;

    // 21:9 — the visible canvas grows from 1920 to 2520 wide.
    lCanvas.SetTargetSize(2520, 1080);
    const UICanvasStats lStats = lCanvas.Update();

    CheckVec(lCorner->GetBounds().Max(), lCornerBefore + Vector2F{ 300.f, 0.f });
    CheckVec(lCentre->GetBounds().Center, lCentreBefore);

    // Root + its two children are resolved (their parent changed); the nested one is NOT, because
    // its parent's rect came out identical. Only the two that moved rebuild.
    CHECK(lStats.Layouts  == 3);
    CHECK(lStats.Rebuilds == 2);
}

TEST_CASE("UICanvas: a same-aspect resize lays out nothing")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    lCanvas.Root().AddChild(MakeUnique<UIImage>())->SetRect(TopRight({ 100.f, 50.f }));
    lCanvas.Update();

    lCanvas.SetTargetSize(1280, 720);
    const UICanvasStats lStats = lCanvas.Update();

    CHECK(lStats.Layouts  == 0);
    CHECK(lStats.Rebuilds == 0);
    CheckVec(lCanvas.GetVisibleBounds().Size(), { 1920.f, 1080.f });
}

TEST_CASE("UICanvas: an idle frame costs 0/0")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    lCanvas.Root().AddChild(MakeUnique<UIImage>());
    lCanvas.Update();

    const UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 0);
    CHECK(lStats.Rebuilds == 0);
}

TEST_CASE("UICanvas: SetRect re-lays that widget and its subtree, never its sibling")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIWidget* lA  = lCanvas.Root().AddChild(MakeUnique<UIPanel>());
    UIWidget* lA1 = lA->AddChild(MakeUnique<UIImage>());
    UIWidget* lB  = lCanvas.Root().AddChild(MakeUnique<UIImage>());
    lCanvas.Update();

    const Bounds2D lBBefore = lB->GetBounds();

    lA->SetRect(Centred({ 300.f, 300.f }, { 100.f, 0.f }));
    const UICanvasStats lStats = lCanvas.Update();

    CHECK(lStats.Layouts  == 2);   // A, A1
    CHECK(lStats.Rebuilds == 2);
    CheckVec(lA1->GetBounds().Center, { 100.f, 0.f });
    CheckVec(lB->GetBounds().Center, lBBefore.Center);
}

TEST_CASE("UICanvas: content invalidation rebuilds without a layout")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    UIImage* lBar = static_cast<UIImage*>(lCanvas.Root().AddChild(MakeUnique<UIImage>()));
    lCanvas.Update();

    lBar->SetFill(EUIFill::Horizontal, 0.25f);
    const UICanvasStats lStats = lCanvas.Update();

    CHECK(lStats.Layouts  == 0);
    CHECK(lStats.Rebuilds == 1);
}

TEST_CASE("UICanvas: AddChild resolves the newcomer alone; RemoveChild resolves nothing")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    lCanvas.Root().AddChild(MakeUnique<UIImage>());
    lCanvas.Update();

    UIWidget* lNew = lCanvas.Root().AddChild(MakeUnique<UIImage>());
    UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 1);
    CHECK(lStats.Rebuilds == 1);

    TUniquePtr<UIWidget> lTaken = lCanvas.Root().RemoveChild(*lNew);
    REQUIRE(lTaken.get() == lNew);
    CHECK(lTaken->GetParent() == nullptr);
    CHECK(lCanvas.Root().GetChildren().size() == 1);

    lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 0);
    CHECK(lStats.Rebuilds == 0);

    // Not mine → nothing happens, nothing is handed back.
    UIImage lStranger;
    CHECK(lCanvas.Root().RemoveChild(lStranger) == nullptr);
}

TEST_CASE("UICanvas: hit-test — the later sibling wins, hidden and pass-through lose, the root never hits")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIWidget* lUnder = lCanvas.Root().AddChild(MakeUnique<UIImage>());
    UIWidget* lOver  = lCanvas.Root().AddChild(MakeUnique<UIImage>());
    lCanvas.Update();

    // Both are the default 100x100 box at the origin.
    CHECK(lCanvas.HitTest({ 0.f, 0.f }) == lOver);
    CHECK(lCanvas.HitTest({ 50.f, 50.f }) == lOver);   // inclusive edge

    lOver->bVisible = false;
    CHECK(lCanvas.HitTest({ 0.f, 0.f }) == lUnder);

    lOver->bVisible     = true;
    lOver->bHitTestable = false;
    CHECK(lCanvas.HitTest({ 0.f, 0.f }) == lUnder);

    // Empty canvas space: the root covers it and must NOT answer (L29).
    CHECK(lCanvas.HitTest({ 800.f, 400.f }) == nullptr);
}

TEST_CASE("UICanvas: hit-test reaches a child outside its parent — nothing clips yet")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIWidget* lParent = lCanvas.Root().AddChild(MakeUnique<UIPanel>());
    UIWidget* lChild  = lParent->AddChild(MakeUnique<UIImage>());
    lChild->SetRect(Centred({ 20.f, 20.f }, { 250.f, 250.f }));
    lCanvas.Update();

    CHECK(lCanvas.HitTest({ 250.f, 250.f }) == lChild);
    CHECK(lCanvas.HitTest({ 0.f, 0.f }) == nullptr);   // the panel is pass-through
}

TEST_CASE("UIImage: one quad on its bounds; a horizontal fill crops the rect and the UVs from the left")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    UIImage* lImage = static_cast<UIImage*>(lCanvas.Root().AddChild(MakeUnique<UIImage>()));
    lImage->SetRect(Centred({ 200.f, 40.f }));
    lImage->SetColor({ 1.f, 0.f, 0.f, 1.f });
    lCanvas.Update();

    REQUIRE(lImage->GetQuads().size() == 1);
    CheckVec(lImage->GetQuads()[0].Bounds.Center, { 0.f, 0.f });
    CheckVec(lImage->GetQuads()[0].Bounds.Size(), { 200.f, 40.f });
    CHECK(lImage->GetQuads()[0].Color.r == doctest::Approx(1.f));
    CHECK(lImage->GetQuads()[0].Texture == nullptr);

    lImage->SetFill(EUIFill::Horizontal, 0.5f);
    lCanvas.Update();

    REQUIRE(lImage->GetQuads().size() == 1);
    const UIQuad& lHalf = lImage->GetQuads()[0];
    CheckVec(lHalf.Bounds.Min(),  { -100.f, -20.f });
    CheckVec(lHalf.Bounds.Size(), { 100.f, 40.f });
    CHECK(lHalf.UVMax.x == doctest::Approx(0.5f));
    CHECK(lHalf.UVMax.y == doctest::Approx(1.f));

    lImage->SetFillAmount(0.f);
    lCanvas.Update();
    CHECK(lImage->GetQuads().empty());
}

TEST_CASE("UICanvas: the view is origin-centred at half the reference height, and pixels map through it")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    const CameraView lView = lCanvas.MakeView();
    CheckVec(lView.Position, { 0.f, 0.f });
    CHECK(lView.OrthoSize == doctest::Approx(540.f));

    CheckVec(lCanvas.ScreenToCanvas({ 960.f, 540.f }), { 0.f, 0.f });
    CheckVec(lCanvas.ScreenToCanvas({ 0.f, 0.f }), { -960.f, 540.f });   // top-left pixel, Y-up

    // A smaller target of the same aspect maps to the SAME canvas point — resolution independence.
    lCanvas.SetTargetSize(1280, 720);
    CheckVec(lCanvas.ScreenToCanvas({ 0.f, 0.f }), { -960.f, 540.f });

    // And back: the editor's outline is drawn through the inverse, so the two must agree — and a
    // 720-tall target shows 1080 units, so a pixel is 1.5 of them (the drag's conversion).
    CheckVec(lCanvas.CanvasToScreen({ -960.f, 540.f }), { 0.f, 0.f });
    CheckVec(lCanvas.CanvasToScreen({ 0.f, 0.f }), { 640.f, 360.f });
    CHECK(lCanvas.UnitsPerPixel() == doctest::Approx(1.5f));
}

namespace
{
    /** A leaf whose input is "not ready" for N rebuilds: it re-arms itself from inside Rebuild. */
    class ReArmingWidget final : public UIWidget
    {
    public:
        Uint32 NotReadyFor = 0;
        Uint32 Rebuilds    = 0;

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("ReArming"); }

    protected:
        void Rebuild(const UIBuildContext&, TDynArray<UIQuad>& OutQuads) override
        {
            ++Rebuilds;
            if (NotReadyFor > 0)
            {
                --NotReadyFor;
                InvalidateContent();   // ask again next frame
                return;
            }
            OutQuads.emplace_back();
        }
    };
}

TEST_CASE("UICanvas: a Rebuild that re-arms is visited again next frame, and the walk reaches it")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    // Two levels down, so the re-arm has to climb through a parent whose flags the walk already cleared.
    UIWidget*       lPanel = lCanvas.Root().AddChild(MakeUnique<UIPanel>());
    ReArmingWidget* lLeaf  = static_cast<ReArmingWidget*>(lPanel->AddChild(MakeUnique<ReArmingWidget>()));
    lLeaf->NotReadyFor = 2;

    lCanvas.Update();
    CHECK(lLeaf->Rebuilds == 1);
    CHECK(lLeaf->GetQuads().empty());

    UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 0);   // nothing moved — only the rebuild is owed
    CHECK(lStats.Rebuilds == 1);
    CHECK(lLeaf->Rebuilds == 2);

    lStats = lCanvas.Update();
    CHECK(lLeaf->Rebuilds == 3);
    CHECK(lLeaf->GetQuads().size() == 1);

    // Ready: the re-arm stops and the canvas goes idle.
    lStats = lCanvas.Update();
    CHECK(lStats.Rebuilds == 0);
    CHECK(lLeaf->Rebuilds == 3);
}

TEST_CASE("UICanvas: a reference-height change re-lays the root")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    lCanvas.Update();

    lCanvas.SetReferenceHeight(720.f);
    const UICanvasStats lStats = lCanvas.Update();

    CHECK(lStats.Layouts == 1);
    CheckVec(lCanvas.Root().GetBounds().Size(), { 1280.f, 720.f });
}
