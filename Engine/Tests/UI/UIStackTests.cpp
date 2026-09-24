// Suite: the layout container (UI/Widgets/UIStack.h) — UI23.
//
// The container owns its children's rects, and that is the whole contract: where each slot lands
// for each axis, alignment and padding; that a hidden child KEEPS its slot; and the invalidation
// numbers the seed reserved for "an ancestor whose size depends on its children" — a child's size
// change re-lays the stack and every sibling, a child's content change re-lays nothing.
#include <doctest.h>

#include "UI/UICanvas.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidgetRegistry.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIStack.h"

using namespace Opaax;

namespace
{
    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x));
        CHECK(InA.y == doctest::Approx(InB.y));
    }

    /** A widget whose desired size is InSize; its anchors are whatever, the stack ignores them. */
    TUniquePtr<UIImage> Sized(const Vector2F& InSize)
    {
        auto lImage = MakeUnique<UIImage>();
        lImage->Rect.SizeDelta        = InSize;
        lImage->Rect.AnchoredPosition = { 999.f, 999.f };   // must have no effect under a stack
        return lImage;
    }

    /** A stack centred on the canvas, InSize big. */
    UIStack* AddStack(UICanvas& InCanvas, const Vector2F& InSize)
    {
        auto lStack = MakeUnique<UIStack>();
        lStack->Rect.SizeDelta = InSize;
        return static_cast<UIStack*>(InCanvas.Root().AddChild(Move(lStack)));
    }
}

TEST_CASE("UIStack: a vertical stack lays its children top-down with spacing and padding, ignoring their anchors")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIStack* lStack = AddStack(lCanvas, { 300.f, 400.f });   // x -150..150, y -200..200
    lStack->Spacing = 10.f;
    lStack->Padding = { 20.f, 20.f, 30.f, 30.f };             // Left, Right, Bottom, Top

    UIWidget* lA = lStack->AddChild(Sized({ 100.f, 50.f }));
    UIWidget* lB = lStack->AddChild(Sized({ 200.f, 80.f }));
    lCanvas.Update();

    // Centred across (the default): inner x is -130..130, so A is -50..50 and B is -100..100.
    // Down from the top: inner top is 170, A takes 170..120, then 10 of spacing, B 110..30.
    CheckVec(lA->GetBounds().Min(), { -50.f, 120.f });
    CheckVec(lA->GetBounds().Max(), {  50.f, 170.f });
    CheckVec(lB->GetBounds().Min(), { -100.f, 30.f });
    CheckVec(lB->GetBounds().Max(), {  100.f, 110.f });

    lStack->SetChildAlign(EUIAlign::Start);
    lCanvas.Update();
    CheckVec(lA->GetBounds().Min(), { -130.f, 120.f });

    lStack->SetChildAlign(EUIAlign::End);
    lCanvas.Update();
    CheckVec(lA->GetBounds().Max(), { 130.f, 170.f });

    lStack->SetChildAlign(EUIAlign::Stretch);
    lCanvas.Update();
    CheckVec(lA->GetBounds().Min(), { -130.f, 120.f });
    CheckVec(lA->GetBounds().Max(), {  130.f, 170.f });
}

TEST_CASE("UIStack: a horizontal stack lays its children left-right, and Start is the TOP across")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIStack* lStack = AddStack(lCanvas, { 400.f, 100.f });   // x -200..200, y -50..50
    lStack->Axis    = EUIAxis::Horizontal;
    lStack->Spacing = 5.f;
    lStack->ChildAlign = EUIAlign::Start;

    UIWidget* lA = lStack->AddChild(Sized({ 60.f, 20.f }));
    UIWidget* lB = lStack->AddChild(Sized({ 40.f, 30.f }));
    lCanvas.Update();

    CheckVec(lA->GetBounds().Min(), { -200.f, 30.f });
    CheckVec(lA->GetBounds().Max(), { -140.f, 50.f });
    CheckVec(lB->GetBounds().Min(), { -135.f, 20.f });
    CheckVec(lB->GetBounds().Max(), {  -95.f, 50.f });
}

TEST_CASE("UIStack: a hidden child KEEPS its slot — visibility never re-lays (UI3)")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIStack* lStack = AddStack(lCanvas, { 300.f, 400.f });
    UIWidget* lA = lStack->AddChild(Sized({ 100.f, 50.f }));
    UIWidget* lB = lStack->AddChild(Sized({ 100.f, 50.f }));
    lCanvas.Update();

    const Bounds2D lBBefore = lB->GetBounds();

    lA->bVisible = false;
    const UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 0);
    CHECK(lStats.Rebuilds == 0);
    CheckVec(lB->GetBounds().Min(), lBBefore.Min());

    // Removing it is what closes the gap: the stack and its remaining child re-lay.
    lStack->RemoveChild(*lA);
    const UICanvasStats lAfter = lCanvas.Update();
    CHECK(lAfter.Layouts == 2);   // the stack, then B into A's old slot
    CheckVec(lB->GetBounds().Max(), { 50.f, 200.f });
}

TEST_CASE("UIStack: a child's SIZE change re-lays the stack and every sibling; its CONTENT change re-lays nothing")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIStack* lStack = AddStack(lCanvas, { 300.f, 400.f });
    UIImage* lA = static_cast<UIImage*>(lStack->AddChild(Sized({ 100.f, 50.f })));
    UIWidget* lB = lStack->AddChild(Sized({ 100.f, 50.f }));
    UIWidget* lC = lStack->AddChild(Sized({ 100.f, 50.f }));
    lCanvas.Update();

    UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts == 0);   // idle stays idle

    // A grows: the up-propagation reaches the stack, which re-lays all three; only B and C MOVE
    // (A's top edge stays put), so A rebuilds for its size and the two others for their position.
    UIRect lBigger = lA->Rect;
    lBigger.SizeDelta = { 100.f, 90.f };
    lA->SetRect(lBigger);

    lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 4);   // stack + A + B + C
    CHECK(lStats.Rebuilds == 4);   // the stack's (empty, as a panel's is) + the three that moved or grew
    CheckVec(lA->GetBounds().Size(), { 100.f, 90.f });
    CheckVec(lB->GetBounds().Max(),  { 50.f, 110.f });
    CheckVec(lC->GetBounds().Max(),  { 50.f, 60.f });

    // A's fill changes: content only, and the stack is not asked to arrange anything.
    lA->SetFill(EUIFill::Horizontal, 0.5f);
    lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 0);
    CHECK(lStats.Rebuilds == 1);
}

TEST_CASE("UIStack: bFitContent sizes the stack along its axis from its children, and grows on AddChild")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    // Anchored to the top-left corner, pivot there too — the row of lives.
    auto lRow = MakeUnique<UIStack>();
    lRow->Axis        = EUIAxis::Horizontal;
    lRow->Spacing     = 4.f;
    lRow->Padding     = { 8.f, 8.f, 0.f, 0.f };
    lRow->bFitContent = true;
    lRow->Rect.AnchorMin = lRow->Rect.AnchorMax = lRow->Rect.Pivot = { 0.f, 1.f };
    lRow->Rect.SizeDelta = { 999.f, 32.f };   // the width is overridden; the height is authored
    UIStack* lStack = static_cast<UIStack*>(lCanvas.Root().AddChild(Move(lRow)));

    lStack->AddChild(Sized({ 24.f, 24.f }));
    lStack->AddChild(Sized({ 24.f, 24.f }));
    lCanvas.Update();

    // 8 + 24 + 4 + 24 + 8 = 68 wide, 32 tall, pinned to the canvas's top-left.
    CheckVec(lStack->GetBounds().Size(), { 68.f, 32.f });
    CheckVec(lStack->GetBounds().Min(),  { -960.f, 540.f - 32.f });

    lStack->AddChild(Sized({ 24.f, 24.f }));
    lCanvas.Update();
    CheckVec(lStack->GetBounds().Size(), { 96.f, 32.f });

    // A stretched axis ignores the fit — the parent sizes it.
    lStack->Rect.AnchorMin = { 0.f, 1.f };
    lStack->Rect.AnchorMax = { 1.f, 1.f };
    lStack->Rect.SizeDelta = { 0.f, 32.f };
    lStack->InvalidateLayout();
    lCanvas.Update();
    CheckVec(lStack->GetBounds().Size(), { 1920.f, 32.f });
}

TEST_CASE("UIStack: stacks nest, and a grandchild's size change climbs through both")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIStack* lOuter = AddStack(lCanvas, { 400.f, 400.f });
    lOuter->ChildAlign = EUIAlign::Start;

    auto lInnerOwned = MakeUnique<UIStack>();
    lInnerOwned->Axis           = EUIAxis::Horizontal;
    lInnerOwned->Rect.SizeDelta = { 300.f, 50.f };
    lInnerOwned->ChildAlign     = EUIAlign::Start;
    UIStack* lInner = static_cast<UIStack*>(lOuter->AddChild(Move(lInnerOwned)));

    UIWidget* lLeaf  = lInner->AddChild(Sized({ 50.f, 50.f }));
    UIWidget* lLeaf2 = lInner->AddChild(Sized({ 50.f, 50.f }));
    UIWidget* lBelow = lOuter->AddChild(Sized({ 100.f, 100.f }));
    lCanvas.Update();

    // The inner stack is the outer's first slot (top-left of -200..200); its leaves run left to right inside it.
    CheckVec(lInner->GetBounds().Min(), { -200.f, 150.f });
    CheckVec(lLeaf->GetBounds().Min(),  { -200.f, 150.f });
    CheckVec(lLeaf2->GetBounds().Min(), { -150.f, 150.f });
    CheckVec(lBelow->GetBounds().Max(), { -100.f, 150.f });

    // A leaf widens: the inner re-arranges its row; the outer is re-laid too (its child asked),
    // and lands on the same slots, so nothing below the inner row moves except the second leaf.
    UIRect lWide = lLeaf->Rect;
    lWide.SizeDelta = { 80.f, 50.f };
    lLeaf->SetRect(lWide);

    const UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Layouts == 5);    // outer, inner, leaf, leaf2, below — all asked, one moved
    CHECK(lStats.Rebuilds == 4);   // both containers (empty), leaf (its size), leaf2 (its position); not below
    CheckVec(lLeaf2->GetBounds().Min(), { -120.f, 150.f });
    CheckVec(lBelow->GetBounds().Max(), { -100.f, 150.f });
}

TEST_CASE("UIStack: the file round-trips every field")
{
    UIWidgetRegistry lRegistry;
    lRegistry.Register<UIPanel>(OPAAX_ID("UIPanel"));
    lRegistry.Register<UIStack>(OPAAX_ID("UIStack"));
    lRegistry.Register<UIImage>(OPAAX_ID("UIImage"));

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = MakeUnique<UIPanel>();

    auto lStack = MakeUnique<UIStack>();
    lStack->Name        = "Menu";
    lStack->Axis        = EUIAxis::Horizontal;
    lStack->Spacing     = 12.f;
    lStack->Padding     = { 1.f, 2.f, 3.f, 4.f };
    lStack->ChildAlign  = EUIAlign::End;
    lStack->bFitContent = true;
    lStack->AddChild(Sized({ 10.f, 10.f }));
    lDoc.Root->AddChild(Move(lStack));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 1u);

    const auto* lRead = dynamic_cast<const UIStack*>(lBack.Root->GetChildren()[0].get());
    REQUIRE(lRead != nullptr);
    CHECK(lRead->Axis == EUIAxis::Horizontal);
    CHECK(lRead->Spacing == doctest::Approx(12.f));
    CHECK(lRead->Padding.Left == doctest::Approx(1.f));
    CHECK(lRead->Padding.Top  == doctest::Approx(4.f));
    CHECK(lRead->ChildAlign == EUIAlign::End);
    CHECK(lRead->bFitContent);
    CHECK(lRead->ArrangesChildren());
    CHECK(lRead->GetChildren().size() == 1u);
}
