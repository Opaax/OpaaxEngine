// Suite: UIButton (UI/Widgets/UIButton.h) — a click is Down then Up while still inside; the quad
// wears the state's colour; disabled falls through like an image.
#include <doctest.h>

#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"

using namespace Opaax;

namespace
{
    UIPointerEvent Ev(const EUIPointerEventType InType, const Vector2F& InAt,
                      const EUIPointerButton InButton = EUIPointerButton::Primary)
    {
        return UIPointerEvent{ InType, InAt, InButton };
    }

    struct Fixture
    {
        UICanvas  Canvas{ 1080.f };
        UIButton* Button = nullptr;   // 200x80 at the origin
        Uint32    Clicks = 0;

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(MakeUnique<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 80.f };
            Button->SetRect(lRect);
            Button->OnClick.Add([this]() { ++Clicks; });
            Canvas.Update();
        }

        Vector4F QuadColor()
        {
            Canvas.Update();
            REQUIRE(Button->GetQuads().size() == 1u);
            return Button->GetQuads()[0].Color;
        }
    };
}

TEST_CASE("UIButton: Down then Up inside clicks once, and both are handled")
{
    Fixture lF;

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f })) == EUIReply::Handled);
    CHECK(lF.Clicks == 0);   // not yet — a press is not a click
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 10.f, 10.f })) == EUIReply::Handled);
    CHECK(lF.Clicks == 1);
}

TEST_CASE("UIButton: released outside is no click, but the Up is still consumed — it captured the press")
{
    Fixture lF;

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 900.f, 500.f })) == EUIReply::Handled);
    CHECK(lF.Clicks == 0);

    // An Up with no prior Down on it: inside, but nothing was pressed.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 0);
}

TEST_CASE("UIButton: only the primary button clicks; a secondary press falls through")
{
    Fixture lF;

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }, EUIPointerButton::Secondary)) == EUIReply::Unhandled);
    CHECK(lF.Canvas.GetPressed() == nullptr);
}

TEST_CASE("UIButton: disabled falls through and wears the Disabled colour")
{
    Fixture lF;
    lF.Button->SetEnabled(false);

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f })) == EUIReply::Unhandled);
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 0);
    CHECK(lF.QuadColor().a == doctest::Approx(lF.Button->Disabled.a));
}

TEST_CASE("UIButton: the quad's colour follows Normal -> Hovered -> Pressed -> Hovered -> Normal")
{
    Fixture lF;
    lF.Button->Normal  = { 0.1f, 0.f, 0.f, 1.f };
    lF.Button->Hovered = { 0.2f, 0.f, 0.f, 1.f };
    lF.Button->Pressed = { 0.3f, 0.f, 0.f, 1.f };
    lF.Button->InvalidateContent();

    CHECK(lF.QuadColor().r == doctest::Approx(0.1f));

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));   // Enter
    CHECK(lF.QuadColor().r == doctest::Approx(0.2f));

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    CHECK(lF.QuadColor().r == doctest::Approx(0.3f));

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.QuadColor().r == doctest::Approx(0.2f));   // still hovered

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 900.f, 500.f }, EUIPointerButton::None));   // Leave
    CHECK(lF.QuadColor().r == doctest::Approx(0.1f));
}
