// Suite: poses (UI/UIPose.h, UITransformModifier). A pose moves, scales, turns, fades and tints a
// widget and everything under it at draw time, without touching its layout. Without one, the draw list
// is the quads' own values: old canvases draw exactly as before.
#include <doctest.h>

#include "Core/Maths/MathsStatics.h"
#include "UI/Modifiers/UITransformModifier.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x).epsilon(1e-4));
        CHECK(InA.y == doctest::Approx(InB.y).epsilon(1e-4));
    }

    UIRect Centred(const Vector2F& InSize, const Vector2F& InOffset = { 0.f, 0.f })
    {
        UIRect lRect;
        lRect.AnchoredPosition = InOffset;
        lRect.SizeDelta        = InSize;
        return lRect;
    }

    UIWidget* AddImage(UIWidget& InParent, const Vector2F& InSize, const Vector2F& InOffset = { 0.f, 0.f })
    {
        UIWidget* lImage = InParent.AddChild(NewObject<UIImage>());
        lImage->SetRect(Centred(InSize, InOffset));
        return lImage;
    }

    UIPose Turned(const float InDegrees)
    {
        UIPose lPose;
        lPose.Rotation = InDegrees;
        return lPose;
    }

    UIPose Scaled(const float InScale)
    {
        UIPose lPose;
        lPose.Scale = { InScale, InScale };
        return lPose;
    }

    struct Fixture
    {
        UICanvas Canvas{ 1080.f };

        Fixture() { Canvas.SetTargetSize(1920, 1080); }

        TDynArray<UIDrawItem> DrawList()
        {
            Canvas.Update();
            TDynArray<UIDrawItem> lItems;
            Canvas.BuildDrawList(lItems);
            return lItems;
        }
    };

    const UIDrawItem* ItemFor(const TDynArray<UIDrawItem>& InItems, const UIWidget& InWidget)
    {
        for (const UIDrawItem& lItem : InItems)
        {
            if (!InWidget.GetQuads().empty() && lItem.Quad == &InWidget.GetQuads()[0]) { return &lItem; }
        }
        return nullptr;
    }
}

TEST_CASE("UIPose: without a pose, every item is its quad as it was")
{
    Fixture lF;
    UIWidget* lImage = AddImage(lF.Canvas.Root(), { 100.f, 40.f }, { 30.f, -20.f });
    UIWidget* lChild = AddImage(*lImage, { 10.f, 10.f });
    lChild->Opacity  = 0.5f;

    const TDynArray<UIDrawItem> lItems = lF.DrawList();
    REQUIRE(lItems.size() == 2u);

    for (const UIDrawItem& lItem : lItems)
    {
        // Exact, not near: the fast way copies the quad's own values.
        CHECK(lItem.Center == lItem.Quad->Bounds.Center);
        CHECK(lItem.Size == lItem.Quad->Bounds.Size());
        CHECK(lItem.Rotation == 0.f);
        CHECK(lItem.Color.x == lItem.Quad->Color.x);
        CHECK(lItem.Color.w == lItem.Quad->Color.w * lItem.Alpha);
    }

    CHECK(lImage->GetPose().bRest);
    CHECK(lImage->GetDrawTransform().IsIdentity());

    // A transform at rest changes nothing either.
    lImage->AddModifier<UITransformModifier>();
    lF.Canvas.Update();
    CHECK(lImage->GetPose().bRest);
}

TEST_CASE("UIPose: a scale and a turn are about the pivot; the layout does not move")
{
    Fixture lF;
    UIWidget* lImage = AddImage(lF.Canvas.Root(), { 100.f, 40.f }, { 200.f, 0.f });

    // At the centre: the centre stays, the size doubles.
    UITransformModifier& lTransform = lImage->AddModifier<UITransformModifier>().SetPose(Scaled(2.f));
    TDynArray<UIDrawItem> lItems = lF.DrawList();
    REQUIRE(lItems.size() == 1u);
    CheckVec(lItems[0].Center, { 200.f, 0.f });
    CheckVec(lItems[0].Size, { 200.f, 80.f });

    // The layout rect is untouched.
    CheckVec(lImage->GetBounds().Center, { 200.f, 0.f });
    CheckVec(lImage->GetBounds().Size(), { 100.f, 40.f });

    // About the left edge: the left edge stays, the centre moves right by half the growth.
    lTransform.SetPivot({ 0.f, 0.5f });
    lItems = lF.DrawList();
    CheckVec(lItems[0].Center, { 250.f, 0.f });

    // A quarter turn about the left edge: the centre goes above it, and the quad keeps its size.
    lTransform.SetPose(Turned(90.f));
    lItems = lF.DrawList();
    CheckVec(lItems[0].Center, { 150.f, 50.f });
    CheckVec(lItems[0].Size, { 100.f, 40.f });
    CHECK(lItems[0].Rotation == doctest::Approx(FHALF_PI));
}

TEST_CASE("UIPose: a moved layout moves the pivot with it")
{
    Fixture lF;
    UIWidget* lImage = AddImage(lF.Canvas.Root(), { 100.f, 40.f });
    lImage->AddModifier<UITransformModifier>().SetPivot({ 0.f, 0.5f }).SetPose(Scaled(2.f));
    lF.DrawList();

    lImage->SetRect(Centred({ 100.f, 40.f }, { 0.f, 300.f }));
    const TDynArray<UIDrawItem> lItems = lF.DrawList();
    CheckVec(lItems[0].Center, { 50.f, 300.f });
}

TEST_CASE("UIPose: a child follows its parent's pose; opacity and tint multiply down")
{
    Fixture lF;
    UIWidget* lParent = AddImage(lF.Canvas.Root(), { 200.f, 200.f });
    UIWidget* lChild  = AddImage(*lParent, { 20.f, 20.f }, { 50.f, 0.f });

    UIPose lPose = Turned(90.f);
    lPose.Opacity = 0.5f;
    lPose.Tint    = LinearColor(1.f, 0.5f, 0.25f, 1.f);
    lParent->Opacity = 0.8f;
    lParent->AddModifier<UITransformModifier>().SetPose(lPose);

    UIPose lChildPose;
    lChildPose.Opacity = 0.5f;
    lChild->AddModifier<UITransformModifier>().SetPose(lChildPose);

    const TDynArray<UIDrawItem> lItems = lF.DrawList();
    const UIDrawItem* lChildItem = ItemFor(lItems, *lChild);
    REQUIRE(lChildItem != nullptr);

    // Right of the parent's centre, turned a quarter: above it.
    CheckVec(lChildItem->Center, { 0.f, 50.f });
    CHECK(lChildItem->Rotation == doctest::Approx(FHALF_PI));

    // 0.8 (parent) x 0.5 (parent's pose) x 0.5 (child's pose).
    CHECK(lChildItem->Alpha == doctest::Approx(0.2f));
    CHECK(lChild->GetDrawnOpacity() == doctest::Approx(0.2f));
    CHECK(lChildItem->Color.y == doctest::Approx(0.5f));
    CHECK(lChildItem->Color.z == doctest::Approx(0.25f));
    CHECK(lChildItem->Color.w == doctest::Approx(0.2f));

    // The child's layout is its own: still right of the centre.
    CheckVec(lChild->GetBounds().Center, { 50.f, 0.f });
    CheckVec(lChild->GetDrawTransform().TransformPoint(lChild->GetBounds().Center), { 0.f, 50.f });
}

TEST_CASE("UIPose: two poses compose in list order, each about its own pivot")
{
    Fixture lF;
    UIWidget* lImage = AddImage(lF.Canvas.Root(), { 100.f, 100.f });

    // Scale by 2 about the centre, then move 10 right: the move is not scaled.
    UIPose lMove;
    lMove.Translation = { 10.f, 0.f };
    UITransformModifier& lFirst = lImage->AddModifier<UITransformModifier>().SetPose(Scaled(2.f));
    lImage->AddModifier<UITransformModifier>().SetPose(lMove);

    TDynArray<UIDrawItem> lItems = lF.DrawList();
    CheckVec(lItems[0].Center, { 10.f, 0.f });
    CheckVec(lItems[0].Size, { 200.f, 200.f });

    // Two scales multiply: hover at 1.05 and press at 0.95 both show.
    lFirst.SetPose(Scaled(1.05f));
    lImage->GetModifiers()[1]->bEnabled = false;
    UITransformModifier& lThird = lImage->AddModifier<UITransformModifier>().SetPose(Scaled(0.95f));
    lItems = lF.DrawList();
    CheckVec(lItems[0].Size, { 99.75f, 99.75f });

    // A disabled modifier is skipped; removing one composes again.
    lImage->RemoveModifier(lThird);
    lItems = lF.DrawList();
    CheckVec(lItems[0].Size, { 105.f, 105.f });
}

TEST_CASE("UIPose: an edit written to the field shows after PostEditChange, as the editor does")
{
    Fixture lF;
    UIWidget* lImage = AddImage(lF.Canvas.Root(), { 100.f, 100.f });
    UITransformModifier& lTransform = lImage->AddModifier<UITransformModifier>();
    lF.DrawList();

    lTransform.Pose.Scale = { 3.f, 3.f };
    lTransform.PostEditChange();
    const TDynArray<UIDrawItem> lItems = lF.DrawList();
    CheckVec(lItems[0].Size, { 300.f, 300.f });
}

TEST_CASE("UIPose: a posed mask clips by its posed box, and a quad outside it is culled")
{
    Fixture lF;

    auto lMaskNode = NewObject<UIMask>();
    lMaskNode->SetRect(Centred({ 100.f, 100.f }));
    UIWidget* lMask = lF.Canvas.Root().AddChild(Move(lMaskNode));

    UIWidget* lInside  = AddImage(*lMask, { 20.f, 20.f });
    UIWidget* lOutside = AddImage(*lMask, { 20.f, 20.f }, { 140.f, 0.f });

    // Not posed: the far one is outside the mask and culled.
    TDynArray<UIDrawItem> lItems = lF.DrawList();
    CHECK(ItemFor(lItems, *lInside) != nullptr);
    CHECK(ItemFor(lItems, *lOutside) == nullptr);

    // Scaled by 3 about its centre: the mask covers 300 x 300 and its children grow with it; the far
    // one, drawn at 420, is still outside.
    lMask->AddModifier<UITransformModifier>().SetPose(Scaled(3.f));
    lItems = lF.DrawList();

    const UIDrawItem* lInsideItem = ItemFor(lItems, *lInside);
    REQUIRE(lInsideItem != nullptr);
    CheckVec(lInsideItem->MaskRect.Size(), { 300.f, 300.f });
    CheckVec(lInsideItem->Size, { 60.f, 60.f });
    CHECK(ItemFor(lItems, *lOutside) == nullptr);

    // Turned 45 degrees, the box covers the turned mask.
    lMask->GetModifiers()[0].Get()->bEnabled = false;
    lMask->AddModifier<UITransformModifier>().SetPose(Turned(45.f));
    lItems = lF.DrawList();
    lInsideItem = ItemFor(lItems, *lInside);
    REQUIRE(lInsideItem != nullptr);
    CHECK(lInsideItem->MaskRect.Size().x == doctest::Approx(100.f * 1.41421356f).epsilon(1e-3));
}

TEST_CASE("UIPose: a modifier removed with its widget leaves nothing behind")
{
    Fixture lF;
    UIWidget* lParent = AddImage(lF.Canvas.Root(), { 100.f, 100.f });
    UIWidget* lChild  = AddImage(*lParent, { 10.f, 10.f });
    lChild->AddModifier<UITransformModifier>().SetPose(Scaled(2.f));
    lF.DrawList();

    // The removed subtree's widgets and modifiers go; the canvas draws on.
    TStrongObjectPtr<UIWidget> lRemoved = lF.Canvas.Root().RemoveChild(*lParent);
    lRemoved.Reset();
    CHECK(lF.DrawList().empty());
}

// =============================================================================
// The hit area (bMovesHitArea)
// =============================================================================

namespace
{
    /** Takes presses and keeps where each pointer event reached it. */
    class UIPointerRecorder final : public UIWidget
    {
        OPAAX_OBJECT(UIPointerRecorder, UIWidget)

    public:
        TDynArray<Vector2F> Positions;

        EUIReply OnPointerEvent(const UIPointerEvent& InEvent) override
        {
            Positions.push_back(InEvent.Position);
            return InEvent.Type == EUIPointerEventType::Down ? EUIReply::Handled : EUIReply::Unhandled;
        }
    };

    UIPose Moved(const Vector2F& InBy)
    {
        UIPose lPose;
        lPose.Translation = InBy;
        return lPose;
    }

    UIPointerEvent Ev(const EUIPointerEventType InType, const Vector2F& InAt)
    {
        return UIPointerEvent{ InType, InAt, EUIPointerButton::Primary };
    }
}

TEST_CASE("UIPose hit area: on, the widget is hit where it is drawn; off, where its layout is")
{
    Fixture lF;
    UIWidget* lMoving = AddImage(lF.Canvas.Root(), { 100.f, 100.f }, { 0.f, 200.f });
    UIWidget* lStaying = AddImage(lF.Canvas.Root(), { 100.f, 100.f }, { 0.f, -200.f });

    lMoving->AddModifier<UITransformModifier>().SetPose(Moved({ 300.f, 0.f }));
    lStaying->AddModifier<UITransformModifier>().SetPose(Moved({ 300.f, 0.f })).SetMovesHitArea(false);
    lF.DrawList();

    // On: at its drawn place only.
    CHECK(lF.Canvas.HitTest({ 300.f, 200.f }) == lMoving);
    CHECK(lF.Canvas.HitTest({ 0.f, 200.f }) == nullptr);

    // Off: drawn at 300, still hit at its layout rect.
    CHECK(lF.Canvas.HitTest({ 0.f, -200.f }) == lStaying);
    CHECK(lF.Canvas.HitTest({ 300.f, -200.f }) == nullptr);

    CHECK(lMoving->GetHitTransform().TransformPoint({ 0.f, 200.f }).x == doctest::Approx(300.f));
    CHECK(lStaying->GetHitTransform().IsIdentity());

    // With every pose off, hit testing is as it was without poses.
    lMoving->GetModifiers()[0].Get()->bEnabled = false;
    lMoving->InvalidatePose();
    lF.DrawList();
    CHECK(lF.Canvas.HitTest({ 0.f, 200.f }) == lMoving);
}

TEST_CASE("UIPose hit area: a widget turned 45 degrees is hit inside its turned box only")
{
    Fixture lF;
    UIWidget* lImage = AddImage(lF.Canvas.Root(), { 100.f, 100.f });
    lImage->AddModifier<UITransformModifier>().SetPose(Turned(45.f));
    lF.DrawList();

    // A corner of the turned square reaches 70.7 along the axis, past the layout's 50.
    CHECK(lF.Canvas.HitTest({ 65.f, 0.f }) == lImage);
    // Inside the layout square's corner, outside the turned one.
    CHECK(lF.Canvas.HitTest({ 45.f, 45.f }) == nullptr);
    CHECK(lF.Canvas.HitTest({ 0.f, 0.f }) == lImage);
}

TEST_CASE("UIPose hit area: a parent's pose moves its children's areas when on")
{
    Fixture lF;
    UIWidget* lParent = AddImage(lF.Canvas.Root(), { 200.f, 200.f });
    lParent->bHitTestable = false;
    UIWidget* lChild = AddImage(*lParent, { 20.f, 20.f }, { 50.f, 0.f });

    UITransformModifier& lTurn = lParent->AddModifier<UITransformModifier>().SetPose(Turned(90.f));
    lF.DrawList();

    // Right of the parent's centre, turned a quarter: above it, and hit there.
    CHECK(lF.Canvas.HitTest({ 0.f, 50.f }) == lChild);
    CHECK(lF.Canvas.HitTest({ 50.f, 0.f }) == nullptr);

    // Off on the parent: the child is hit at its layout, though drawn above.
    lTurn.SetMovesHitArea(false);
    lF.DrawList();
    CHECK(lF.Canvas.HitTest({ 50.f, 0.f }) == lChild);
    CHECK(lF.Canvas.HitTest({ 0.f, 50.f }) == nullptr);
}

TEST_CASE("UIPose hit area: a scale of 0 makes nothing hit, and a mask clips in its posed space")
{
    Fixture lF;
    UIWidget* lImage = AddImage(lF.Canvas.Root(), { 100.f, 100.f });
    lImage->AddModifier<UITransformModifier>().SetPose(Scaled(0.f));
    lF.DrawList();
    CHECK(lF.Canvas.HitTest({ 0.f, 0.f }) == nullptr);

    auto lMaskNode = NewObject<UIMask>();
    lMaskNode->SetRect(Centred({ 100.f, 100.f }, { 400.f, 0.f }));
    UIWidget* lMask  = lF.Canvas.Root().AddChild(Move(lMaskNode));
    UIWidget* lInner = AddImage(*lMask, { 300.f, 40.f });   // wider than the mask
    lMask->AddModifier<UITransformModifier>().SetPose(Scaled(2.f));
    lF.DrawList();

    // Scaled by 2 the mask covers 300..500 in x: the inner bar is hit there, not beyond.
    CHECK(lF.Canvas.HitTest({ 490.f, 0.f }) == lInner);
    CHECK(lF.Canvas.HitTest({ 520.f, 0.f }) == nullptr);
}

TEST_CASE("UIPose hit area: pointer events reach a widget in its layout space")
{
    Fixture lF;
    UIPointerRecorder* lRecorder = static_cast<UIPointerRecorder*>(lF.Canvas.Root().AddChild(NewObject<UIPointerRecorder>()));
    lRecorder->SetRect(Centred({ 100.f, 100.f }));
    lRecorder->AddModifier<UITransformModifier>().SetPose(Scaled(2.f));
    lF.DrawList();

    // At 80 in canvas units, 40 in its own: a drag reads layout units, not screen ones (a scroll box
    // scaled by 2 moves its content as far as the pointer moved over it).
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 80.f, 0.f }));   // Enter
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 80.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 90.f, 20.f }));  // captured
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 90.f, 20.f }));

    REQUIRE(lRecorder->Positions.size() >= 3u);
    for (const Vector2F& lAt : lRecorder->Positions)
    {
        CHECK(lAt.x <= 45.f);   // never the canvas's 80 or 90
    }
    CheckVec(lRecorder->Positions.front(), { 40.f, 0.f });
    CheckVec(lRecorder->Positions.back(), { 45.f, 10.f });
}

TEST_CASE("UIPose hit area: a button moved by a pose clicks where it is drawn")
{
    Fixture lF;
    UIButton* lButton = static_cast<UIButton*>(lF.Canvas.Root().AddChild(NewObject<UIButton>()));
    lButton->SetRect(Centred({ 200.f, 80.f }));
    Uint32 lClicks = 0;
    lButton->OnClick.Add([&lClicks]() { ++lClicks; });

    lButton->AddModifier<UITransformModifier>().SetPose(Moved({ 300.f, 0.f }));
    lF.DrawList();

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 300.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 300.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 310.f, 5.f }));
    CHECK(lClicks == 1u);

    // Released past its drawn edge: no click, as without a pose.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 300.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 450.f, 0.f }));
    CHECK(lClicks == 1u);
}
