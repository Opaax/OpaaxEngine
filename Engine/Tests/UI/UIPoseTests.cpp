// Suite: poses (UI/UIPose.h, UITransformModifier). A pose moves, scales, turns, fades and tints a
// widget and everything under it at draw time, without touching its layout. Without one, the draw list
// is the quads' own values: old canvases draw exactly as before.
#include <doctest.h>

#include "Core/Maths/MathsStatics.h"
#include "UI/Modifiers/UITransformModifier.h"
#include "UI/UICanvas.h"
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
