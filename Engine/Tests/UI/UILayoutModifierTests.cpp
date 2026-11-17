// Suite: layout modifiers (UIAspectRatioModifier, UISizeLimitModifier). They change a widget's rect
// right after it resolves, about the rect's pivot, and what it asks a stack for, so a stack or a scroll
// box reserves the limited size. Their edits lay the widget out again.
#include <doctest.h>

#include "UI/Modifiers/UIAspectRatioModifier.h"
#include "UI/Modifiers/UISizeLimitModifier.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIScrollBox.h"
#include "UI/Widgets/UIStack.h"

using namespace Opaax;

namespace
{
    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x).epsilon(1e-4));
        CHECK(InA.y == doctest::Approx(InB.y).epsilon(1e-4));
    }

    UIRect Sized(const Vector2F& InSize, const Vector2F& InOffset = { 0.f, 0.f }, const Vector2F& InPivot = { 0.5f, 0.5f })
    {
        UIRect lRect;
        lRect.Pivot            = InPivot;
        lRect.AnchoredPosition = InOffset;
        lRect.SizeDelta        = InSize;
        return lRect;
    }

    UIRect Stretched()
    {
        UIRect lRect;
        lRect.AnchorMin = { 0.f, 0.f };
        lRect.AnchorMax = { 1.f, 1.f };
        lRect.SizeDelta = { 0.f, 0.f };
        return lRect;
    }

    struct Fixture
    {
        UICanvas  Canvas{ 1080.f };
        UIWidget* Panel = nullptr;   // 400 x 400 at the centre
        UIWidget* Image = nullptr;

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Panel = Canvas.Root().AddChild(NewObject<UIPanel>());
            Panel->SetRect(Sized({ 400.f, 400.f }));
            Image = Panel->AddChild(NewObject<UIImage>());
            Image->SetRect(Stretched());
            Canvas.Update();
        }

        const Bounds2D& Bounds()
        {
            Canvas.Update();
            return Image->GetBounds();
        }
    };
}

TEST_CASE("AspectSize: each mode at 16:9")
{
    const float lRatio = 16.f / 9.f;
    CheckVec(AspectSize(EUIAspectMode::WidthControlsHeight, lRatio, { 160.f, 10.f }, { 0.f, 0.f }), { 160.f, 90.f });
    CheckVec(AspectSize(EUIAspectMode::HeightControlsWidth, lRatio, { 10.f, 90.f }, { 0.f, 0.f }), { 160.f, 90.f });

    // A wide parent: fit by height, cover by width. A tall one: the other way round.
    CheckVec(AspectSize(EUIAspectMode::FitInParent, lRatio, { 1.f, 1.f }, { 800.f, 90.f }), { 160.f, 90.f });
    CheckVec(AspectSize(EUIAspectMode::EnvelopeParent, lRatio, { 1.f, 1.f }, { 800.f, 90.f }), { 800.f, 450.f });
    CheckVec(AspectSize(EUIAspectMode::FitInParent, lRatio, { 1.f, 1.f }, { 160.f, 900.f }), { 160.f, 90.f });
    CheckVec(AspectSize(EUIAspectMode::EnvelopeParent, lRatio, { 1.f, 1.f }, { 160.f, 900.f }), { 1600.f, 900.f });

    // No ratio, or no parent to fit: unchanged.
    CheckVec(AspectSize(EUIAspectMode::WidthControlsHeight, 0.f, { 12.f, 34.f }, { 0.f, 0.f }), { 12.f, 34.f });
    CheckVec(AspectSize(EUIAspectMode::FitInParent, lRatio, { 12.f, 34.f }, { 0.f, 0.f }), { 12.f, 34.f });
}

TEST_CASE("UIAspectRatioModifier: a stretched image fits 16:9 in a square panel, centred; envelope covers it")
{
    Fixture lF;
    CheckVec(lF.Bounds().Size(), { 400.f, 400.f });

    UIAspectRatioModifier& lAspect = lF.Image->AddModifier<UIAspectRatioModifier>();
    CheckVec(lF.Bounds().Size(), { 400.f, 225.f });
    CheckVec(lF.Bounds().Center, { 0.f, 0.f });

    lAspect.SetMode(EUIAspectMode::EnvelopeParent);
    CheckVec(lF.Bounds().Size(), { 400.f * 16.f / 9.f, 400.f });
    CheckVec(lF.Bounds().Center, { 0.f, 0.f });
}

TEST_CASE("UIAspectRatioModifier: one side follows the other, about the rect's pivot")
{
    Fixture lF;
    lF.Image->SetRect(Sized({ 160.f, 10.f }, { -100.f, 100.f }, { 0.f, 1.f }));   // top-left at (-100, 100)
    lF.Image->AddModifier<UIAspectRatioModifier>().SetMode(EUIAspectMode::WidthControlsHeight);

    const Bounds2D lBounds = lF.Bounds();
    CheckVec(lBounds.Size(), { 160.f, 90.f });
    // The pivot, the top-left corner, stayed put.
    CHECK(lBounds.Min().x == doctest::Approx(-100.f));
    CHECK(lBounds.Max().y == doctest::Approx(100.f));
}

TEST_CASE("UISizeLimitModifier: min and max per axis, 0 for none, about the pivot")
{
    CheckVec(LimitSize({ 300.f, 20.f }, { 0.f, 40.f }, { 200.f, 0.f }), { 200.f, 40.f });
    CheckVec(LimitSize({ 50.f, 50.f }, { 0.f, 0.f }, { 0.f, 0.f }), { 50.f, 50.f });
    CheckVec(LimitSize({ 50.f, 50.f }, { 100.f, 0.f }, { 80.f, 0.f }), { 80.f, 50.f });   // a max under the min wins

    Fixture lF;
    lF.Image->SetRect(Sized({ 300.f, 20.f }, { 0.f, 0.f }, { 1.f, 0.f }));   // its right-bottom corner at the centre
    lF.Image->AddModifier<UISizeLimitModifier>().SetMinSize({ 0.f, 40.f }).SetMaxSize({ 200.f, 0.f });

    const Bounds2D lBounds = lF.Bounds();
    CheckVec(lBounds.Size(), { 200.f, 40.f });
    CHECK(lBounds.Max().x == doctest::Approx(0.f));
    CHECK(lBounds.Min().y == doctest::Approx(0.f));
}

TEST_CASE("UISizeLimitModifier: a stack and a scroll box reserve the limited size")
{
    UICanvas lCanvas{ 1080.f };
    lCanvas.SetTargetSize(1920, 1080);

    auto lStackNode = NewObject<UIStack>();
    lStackNode->Rect.SizeDelta = { 300.f, 600.f };
    UIStack* lStack = static_cast<UIStack*>(lCanvas.Root().AddChild(Move(lStackNode)));

    UIWidget* lRows[3] = {};
    for (UIWidget*& lRow : lRows)
    {
        lRow = lStack->AddChild(NewObject<UIImage>());
        lRow->Rect.SizeDelta = { 100.f, 100.f };
    }
    lRows[1]->AddModifier<UISizeLimitModifier>().SetMaxSize({ 0.f, 40.f });
    lCanvas.Update();

    // Top-down from 300: 100, then 40, then 100.
    CHECK(lRows[1]->GetDesiredSize().y == doctest::Approx(40.f));
    CHECK(lRows[1]->GetBounds().Size().y == doctest::Approx(40.f));
    CHECK(lRows[2]->GetBounds().Max().y == doctest::Approx(300.f - 140.f));

    // A scroll box 200 tall with 100 + 40 + 100 of rows scrolls 40, not 100.
    auto lBoxNode = NewObject<UIScrollBox>();
    lBoxNode->Rect.SizeDelta = { 300.f, 200.f };
    UIScrollBox* lBox = static_cast<UIScrollBox*>(lCanvas.Root().AddChild(Move(lBoxNode)));
    for (Int32 lIndex = 0; lIndex < 3; ++lIndex)
    {
        UIWidget* lRow = lBox->AddChild(NewObject<UIImage>());
        lRow->Rect.SizeDelta = { 100.f, 100.f };
        if (lIndex == 1) { lRow->AddModifier<UISizeLimitModifier>().SetMaxSize({ 0.f, 40.f }); }
    }
    lCanvas.Update();
    lCanvas.Update();
    CHECK(lBox->GetMaxScroll() == doctest::Approx(40.f));
}

TEST_CASE("Layout modifiers: an edit lays the widget out again; an idle frame lays out nothing")
{
    Fixture lF;
    UISizeLimitModifier& lLimit = lF.Image->AddModifier<UISizeLimitModifier>();
    lF.Bounds();
    CHECK(lF.Canvas.Update().Layouts == 0u);

    lLimit.MaxSize = { 100.f, 0.f };   // the editor writes the field, then says so
    lLimit.PostEditChange();
    CHECK(lF.Canvas.Update().Layouts > 0u);
    CHECK(lF.Image->GetBounds().Size().x == doctest::Approx(100.f));

    // Disabled: as if it were not there.
    lLimit.bEnabled = false;
    lLimit.PostEditChange();
    CheckVec(lF.Bounds().Size(), { 400.f, 400.f });
}
