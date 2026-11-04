// Suite: UIScrollBox (UI/Widgets/UIScrollBox.h) and the layout it shares with UIStack
// (UI/UIStackLayout.h). Where rows land as it scrolls, the wheel through the router (a notch scrolls
// it once), dragging, the clip (a row scrolled away is neither drawn nor hit) and its file.
#include <doctest.h>

#include "Input/InputCodes.h"
#include "Input/InputManager.h"
#include "Object/ObjectClassRegistry.h"
#include "UI/UICanvas.h"
#include "UI/UICanvasFile.h"
#include "UI/UIInputRouter.h"
#include "UI/UIStackLayout.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIScrollBox.h"

using namespace Opaax;

namespace
{
    constexpr Uint16 Idx(const EKeyCode InKey) { return static_cast<Uint16>(InKey); }

    /** A row InHeight tall; a box stretches it across. */
    TStrongObjectPtr<UIImage> Row(const float InHeight)
    {
        auto lRow = NewObject<UIImage>();
        lRow->Rect.SizeDelta = { 10.f, InHeight };
        return lRow;
    }

    /**
     * A 300x200 box at the canvas origin (x -150..150, y -100..100) holding InRows rows 60 tall. On a
     * 1920x1080 target a canvas unit is a pixel, and the origin is pixel (960, 540).
     */
    struct Fixture
    {
        UICanvas            Canvas{ 1080.f };
        InputManager        Input;
        InputKeyMask        Consumed{};
        UIScrollBox*        Box = nullptr;
        TDynArray<UIImage*> Rows;

        explicit Fixture(const Uint32 InRows = 5)
        {
            Canvas.SetTargetSize(1920, 1080);

            auto lBox = NewObject<UIScrollBox>();
            lBox->Rect.SizeDelta = { 300.f, 200.f };
            Box = static_cast<UIScrollBox*>(Canvas.Root().AddChild(Move(lBox)));

            for (Uint32 lIndex = 0; lIndex < InRows; ++lIndex)
            {
                Rows.push_back(static_cast<UIImage*>(Box->AddChild(Row(60.f))));
            }
            Canvas.Update();
        }

        /** One frame of input through the router, then the layout it changed. */
        void Frame()
        {
            UIInputRouter::Route(Input, Canvas, EUIInputMode::GameAndUI, Consumed);
            Input.EndFrame();
            Canvas.Update();
        }

        float Top(const Uint32 InRow) const { return Rows[InRow]->GetBounds().Max().y; }
    };
}

TEST_CASE("UIStackLayout: a scroll moves every slot toward the start, up or left")
{
    const Bounds2D            lBounds = Bounds2D::FromMinMax({ -50.f, -50.f }, { 50.f, 50.f });
    const TDynArray<Vector2F> lSizes  = { { 20.f, 20.f }, { 20.f, 20.f } };

    UIStackLayout lLayout;
    lLayout.Spacing = 5.f;

    TDynArray<Bounds2D> lSlots;
    lLayout.Arrange(lBounds, lSizes, 10.f, lSlots);
    REQUIRE(lSlots.size() == 2u);
    CHECK(lSlots[0].Max().y == doctest::Approx(60.f));   // the top, 10 past it
    CHECK(lSlots[1].Max().y == doctest::Approx(35.f));   // 20 down, 5 between

    lLayout.Axis = EUIAxis::Horizontal;
    lSlots.clear();
    lLayout.Arrange(lBounds, lSizes, 10.f, lSlots);
    CHECK(lSlots[0].Min().x == doctest::Approx(-60.f));   // the left, 10 before it
    CHECK(lLayout.ContentExtent(lSizes) == doctest::Approx(45.f));
}

TEST_CASE("UIScrollBox: lays its rows out like a stack from the top, and scrolls nothing until asked")
{
    Fixture lF;

    CHECK(lF.Top(0) == doctest::Approx(100.f));
    CHECK(lF.Top(1) == doctest::Approx(40.f));
    CHECK(lF.Rows[0]->GetBounds().Min().x == doctest::Approx(-150.f));   // stretched across
    CHECK(lF.Rows[0]->GetBounds().Max().x == doctest::Approx(150.f));

    CHECK(lF.Box->GetMaxScroll() == doctest::Approx(100.f));   // 5 x 60 in a 200 box
    CHECK(lF.Box->GetScroll() == doctest::Approx(0.f));
    CHECK(lF.Box->ClipsChildren());
    CHECK(lF.Box->ArrangesChildren());
}

TEST_CASE("UIScrollBox: SetScroll moves the rows up, kept between 0 and what does not fit")
{
    Fixture lF;

    lF.Box->SetScroll(40.f);
    lF.Canvas.Update();
    CHECK(lF.Box->GetScroll() == doctest::Approx(40.f));
    CHECK(lF.Top(0) == doctest::Approx(140.f));

    lF.Box->SetScroll(500.f);
    lF.Canvas.Update();
    CHECK(lF.Box->GetScroll() == doctest::Approx(100.f));
    CHECK(lF.Rows[4]->GetBounds().Min().y == doctest::Approx(-100.f));   // the last row ends at the bottom

    lF.Box->SetScroll(-10.f);
    lF.Canvas.Update();
    CHECK(lF.Box->GetScroll() == doctest::Approx(0.f));
}

TEST_CASE("UIScrollBox: a notch over a row scrolls the box once by ScrollStep, and the game gets no wheel code")
{
    Fixture lF;
    lF.Input.OnMouseMoved(960.f, 530.f);   // canvas (0, 10): over row 1, which lets the wheel bubble up

    lF.Input.OnMouseScrolled(0.f, -1.f);
    lF.Frame();
    CHECK(lF.Box->GetScroll() == doctest::Approx(60.f));
    CHECK(lF.Top(0) == doctest::Approx(160.f));
    CHECK(lF.Consumed[Idx(EKeyCode::Mouse_WheelDown)]);
    CHECK(lF.Consumed[Idx(EKeyCode::Mouse_WheelAxis)]);

    // Once: no notch, no move.
    lF.Frame();
    CHECK(lF.Box->GetScroll() == doctest::Approx(60.f));

    // At the end it stops, and still keeps the wheel from the game.
    lF.Input.OnMouseScrolled(0.f, -1.f);
    lF.Frame();
    CHECK(lF.Box->GetScroll() == doctest::Approx(100.f));
    lF.Input.OnMouseScrolled(0.f, -1.f);
    lF.Frame();
    CHECK(lF.Box->GetScroll() == doctest::Approx(100.f));
    CHECK(lF.Consumed[Idx(EKeyCode::Mouse_WheelDown)]);

    lF.Input.OnMouseScrolled(0.f, 1.f);
    lF.Frame();
    CHECK(lF.Box->GetScroll() == doctest::Approx(40.f));
}

TEST_CASE("UIScrollBox: when all of its content shows, the wheel and a press go through")
{
    Fixture lF(2);   // 120 in a 200 box
    CHECK(lF.Box->GetMaxScroll() == doctest::Approx(0.f));

    lF.Input.OnMouseMoved(960.f, 540.f);
    lF.Input.OnMouseScrolled(0.f, -1.f);
    lF.Input.OnMouseButtonPressed(EKeyCode::Mouse_Left);
    lF.Frame();

    CHECK(lF.Box->GetScroll() == doctest::Approx(0.f));
    CHECK_FALSE(lF.Consumed[Idx(EKeyCode::Mouse_WheelDown)]);
    CHECK_FALSE(lF.Consumed[Idx(EKeyCode::Mouse_Left)]);
    CHECK(lF.Canvas.GetPressed() == nullptr);
}

TEST_CASE("UIScrollBox: dragging moves the content with the pointer, until the release")
{
    Fixture lF;

    // A press on a row bubbles up to the box, which captures the pointer.
    CHECK(lF.Canvas.RoutePointer({ EUIPointerEventType::Down, { 0.f, 0.f }, EUIPointerButton::Primary })
          == EUIReply::Handled);
    CHECK(lF.Canvas.GetPressed() == lF.Box);
    CHECK(lF.Box->IsDragging());

    // Up 30: the content goes up with it.
    lF.Canvas.RoutePointer({ EUIPointerEventType::Move, { 0.f, 30.f }, EUIPointerButton::None });
    lF.Canvas.Update();
    CHECK(lF.Box->GetScroll() == doctest::Approx(30.f));
    CHECK(lF.Top(0) == doctest::Approx(130.f));

    // Past either end it stops there.
    lF.Canvas.RoutePointer({ EUIPointerEventType::Move, { 400.f, 500.f }, EUIPointerButton::None });
    CHECK(lF.Box->GetScroll() == doctest::Approx(100.f));
    lF.Canvas.RoutePointer({ EUIPointerEventType::Move, { 0.f, -500.f }, EUIPointerButton::None });
    CHECK(lF.Box->GetScroll() == doctest::Approx(0.f));

    // Released outside the box: the drag still ends there, and later moves scroll nothing.
    lF.Canvas.RoutePointer({ EUIPointerEventType::Move, { 0.f, 20.f }, EUIPointerButton::None });
    CHECK(lF.Canvas.RoutePointer({ EUIPointerEventType::Up, { 700.f, 400.f }, EUIPointerButton::Primary })
          == EUIReply::Handled);
    CHECK_FALSE(lF.Box->IsDragging());
    CHECK(lF.Canvas.GetPressed() == nullptr);
    lF.Canvas.RoutePointer({ EUIPointerEventType::Move, { 0.f, 90.f }, EUIPointerButton::None });
    CHECK(lF.Box->GetScroll() == doctest::Approx(20.f));

    // Turned off, a press goes through.
    lF.Box->bDragToScroll = false;
    CHECK(lF.Canvas.RoutePointer({ EUIPointerEventType::Down, { 0.f, 0.f }, EUIPointerButton::Primary })
          == EUIReply::Unhandled);
}

TEST_CASE("UIScrollBox: a row scrolled out of the box is neither drawn nor hit")
{
    Fixture lF;

    TDynArray<UIDrawItem> lItems;
    lF.Canvas.BuildDrawList(lItems);

    // Rows 0 to 3 show (row 3 in part), each masked by the box; row 4 is wholly below it.
    REQUIRE(lItems.size() == 4u);
    for (const UIDrawItem& lItem : lItems)
    {
        CHECK(lItem.Mask == lF.Box);
        CHECK(lItem.Quad != &lF.Rows[4]->GetQuads()[0]);
    }

    CHECK(lF.Canvas.HitTest({ 0.f, -90.f }) == lF.Rows[3]);   // inside the box
    CHECK(lF.Canvas.HitTest({ 0.f, -150.f }) == nullptr);     // row 4's place, outside it

    // Scrolled to the end, row 4 shows and row 0 is gone.
    lF.Box->SetScroll(100.f);
    lF.Canvas.Update();
    lF.Canvas.BuildDrawList(lItems);
    REQUIRE(lItems.size() == 4u);
    CHECK(lItems.back().Quad == &lF.Rows[4]->GetQuads()[0]);
    CHECK(lF.Canvas.HitTest({ 0.f, -90.f }) == lF.Rows[4]);
}

TEST_CASE("UIScrollBox: a horizontal box scrolls left to right, and takes a sideways wheel")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    auto lBoxNode = NewObject<UIScrollBox>();
    lBoxNode->Rect.SizeDelta = { 300.f, 100.f };
    lBoxNode->Axis           = EUIAxis::Horizontal;
    UIScrollBox* const lBox  = static_cast<UIScrollBox*>(lCanvas.Root().AddChild(Move(lBoxNode)));

    UIWidget* lFirst = nullptr;
    for (Uint32 lIndex = 0; lIndex < 5; ++lIndex)
    {
        auto lColumn = NewObject<UIImage>();
        lColumn->Rect.SizeDelta = { 100.f, 10.f };
        UIWidget* const lAdded = lBox->AddChild(Move(lColumn));
        if (lFirst == nullptr) { lFirst = lAdded; }
    }
    lCanvas.Update();

    CHECK(lBox->GetMaxScroll() == doctest::Approx(200.f));
    CHECK(lFirst->GetBounds().Min().x == doctest::Approx(-150.f));

    UIPointerEvent lWheel{ EUIPointerEventType::Scroll, { 0.f, 0.f }, EUIPointerButton::None, { -1.f, 0.f } };
    CHECK(lCanvas.RoutePointer(lWheel) == EUIReply::Handled);
    lCanvas.Update();
    CHECK(lBox->GetScroll() == doctest::Approx(60.f));
    CHECK(lFirst->GetBounds().Min().x == doctest::Approx(-210.f));   // the content went left

    // No sideways turn: the usual wheel moves it along.
    lWheel.Scroll = { 0.f, -1.f };
    lCanvas.RoutePointer(lWheel);
    CHECK(lBox->GetScroll() == doctest::Approx(120.f));
}

TEST_CASE("UIScrollBox: the file round-trips every field")
{
    ObjectClassRegistry lRegistry;
    lRegistry.Register<UIPanel>();
    lRegistry.Register<UIScrollBox>();
    lRegistry.Register<UIImage>();

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = NewObject<UIPanel>();

    auto lBox = NewObject<UIScrollBox>();
    lBox->Name          = "Scores";
    lBox->Axis          = EUIAxis::Horizontal;
    lBox->Spacing       = 8.f;
    lBox->Padding       = { 1.f, 2.f, 3.f, 4.f };
    lBox->ChildAlign    = EUIAlign::Start;
    lBox->ScrollStep    = 25.f;
    lBox->bDragToScroll = false;
    lBox->AddChild(Row(30.f));
    lDoc.Root->AddChild(Move(lBox));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 1u);

    const auto* lRead = dynamic_cast<const UIScrollBox*>(lBack.Root->GetChildren()[0].Get());
    REQUIRE(lRead != nullptr);
    CHECK(lRead->Name == OpaaxString("Scores"));
    CHECK(lRead->Axis == EUIAxis::Horizontal);
    CHECK(lRead->Spacing == doctest::Approx(8.f));
    CHECK(lRead->Padding.Bottom == doctest::Approx(3.f));
    CHECK(lRead->ChildAlign == EUIAlign::Start);
    CHECK(lRead->ScrollStep == doctest::Approx(25.f));
    CHECK_FALSE(lRead->bDragToScroll);
    CHECK(lRead->ClipsChildren());
    CHECK(lRead->GetChildren().size() == 1u);
}
