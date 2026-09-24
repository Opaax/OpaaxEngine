// Suite: UIInputRouter (Engine/UI/UIInputRouter.h) — one frame of UI input, per mode.
//
// A REAL InputManager driven by its own feed and a REAL canvas with a button under the pointer:
// the case each mode has to get right is which keys the game still hears afterwards. No game, no
// world, no GL — the same reason InputActionEvaluator is tested apart from its subsystem.
#include <doctest.h>

#include "Engine/Subsystems/Input/InputCodes.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "Engine/UI/UIInputRouter.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"

using namespace Opaax;

namespace
{
    constexpr Uint16 Idx(const EKeyCode InKey) { return static_cast<Uint16>(InKey); }

    /** Takes any key it is asked while focused. */
    class KeyEater final : public UIWidget
    {
    public:
        Uint32 Keys = 0;
        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("KeyEater"); }
        EUIReply OnKeyEvent(const UIKeyEvent&) override { ++Keys; return EUIReply::Handled; }
    };

    struct Fixture
    {
        InputManager Input;
        UICanvas     Canvas{ 1080.f };
        UIButton*    Button = nullptr;   // 200x80 at the canvas origin = pixel (960, 540)
        KeyEater*    Eater  = nullptr;
        Uint32       Clicks = 0;
        InputKeyMask Consumed{};

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(MakeUnique<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 80.f };
            Button->SetRect(lRect);
            Button->OnClick.Add([this]() { ++Clicks; });
            Eater = static_cast<KeyEater*>(Canvas.Root().AddChild(MakeUnique<KeyEater>()));
            Eater->bHitTestable = false;   // a key sink only — it sits over the button and must not shadow it
            Canvas.Update();
        }

        void Frame(const EUIInputMode InMode)
        {
            UIInputRouter::Route(Input, Canvas, InMode, Consumed);
            Input.EndFrame();
        }

        Uint32 ConsumedCount() const
        {
            Uint32 lCount = 0;
            for (const bool lFlag : Consumed) { if (lFlag) { ++lCount; } }
            return lCount;
        }
    };
}

TEST_CASE("UIInputRouter: GameOnly routes nothing and consumes nothing, even with a button under a press")
{
    Fixture lF;
    lF.Input.OnMouseMoved(960.f, 540.f);
    lF.Input.OnMouseButtonPressed(EKeyCode::Mouse_Left);

    lF.Frame(EUIInputMode::GameOnly);
    CHECK(lF.ConsumedCount() == 0);
    CHECK(lF.Canvas.GetHovered() == nullptr);
    CHECK(lF.Canvas.GetPressed() == nullptr);
}

TEST_CASE("UIInputRouter: GameAndUI consumes the button the widget took — on press, while held, on release — and never Space")
{
    Fixture lF;
    lF.Input.OnMouseMoved(960.f, 540.f);
    lF.Input.OnMouseButtonPressed(EKeyCode::Mouse_Left);
    lF.Input.OnKeyPressed(EKeyCode::Space, false);

    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.Consumed[Idx(EKeyCode::Mouse_Left)]);
    CHECK_FALSE(lF.Consumed[Idx(EKeyCode::Space)]);   // nothing has focus; Space is the game's
    CHECK(lF.Canvas.GetPressed() == lF.Button);

    // Held: still spoken for, so a drag off the button never leaks mid-gesture.
    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.Consumed[Idx(EKeyCode::Mouse_Left)]);

    lF.Input.OnMouseButtonReleased(EKeyCode::Mouse_Left);
    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.Consumed[Idx(EKeyCode::Mouse_Left)]);
    CHECK(lF.Clicks == 1);

    // Nothing pressed, nothing under the pointer: a clean frame consumes nothing.
    lF.Input.OnMouseMoved(0.f, 0.f);
    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.ConsumedCount() == 0);
}

TEST_CASE("UIInputRouter: GameAndUI with the press OFF every widget consumes nothing")
{
    Fixture lF;
    lF.Input.OnMouseMoved(10.f, 10.f);   // top-left corner, far from the button
    lF.Input.OnMouseButtonPressed(EKeyCode::Mouse_Left);

    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.ConsumedCount() == 0);
    CHECK(lF.Canvas.GetPressed() == nullptr);
}

TEST_CASE("UIInputRouter: UIOnly still routes (the click lands) and mutes the game whole")
{
    Fixture lF;
    lF.Input.OnMouseMoved(960.f, 540.f);
    lF.Input.OnMouseButtonPressed(EKeyCode::Mouse_Left);

    lF.Frame(EUIInputMode::UIOnly);
    CHECK(lF.ConsumedCount() == InputManager::KEY_STATE_COUNT);
    CHECK(lF.Canvas.GetPressed() == lF.Button);

    lF.Input.OnMouseButtonReleased(EKeyCode::Mouse_Left);
    lF.Frame(EUIInputMode::UIOnly);
    CHECK(lF.Clicks == 1);
    CHECK(lF.ConsumedCount() == InputManager::KEY_STATE_COUNT);   // an idle UIOnly frame too
}

TEST_CASE("UIInputRouter: a key edge reaches the focused widget and is consumed only when handled")
{
    Fixture lF;

    lF.Input.OnKeyPressed(EKeyCode::Escape, false);
    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.Eater->Keys == 0);   // no focus
    CHECK_FALSE(lF.Consumed[Idx(EKeyCode::Escape)]);

    lF.Canvas.SetFocus(lF.Eater);
    lF.Input.OnKeyReleased(EKeyCode::Escape);
    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.Eater->Keys == 1);   // the release edge
    CHECK(lF.Consumed[Idx(EKeyCode::Escape)]);

    // A held key with no edge is not an event.
    lF.Input.OnKeyPressed(EKeyCode::Escape, false);
    lF.Frame(EUIInputMode::GameAndUI);
    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.Eater->Keys == 2);
}

TEST_CASE("UIInputRouter: the pointer is the canvas's, so the same pixel hits at any target size")
{
    Fixture lF;
    lF.Canvas.SetTargetSize(1280, 720);
    lF.Canvas.Update();

    lF.Input.OnMouseMoved(640.f, 360.f);   // this target's centre
    lF.Input.OnMouseButtonPressed(EKeyCode::Mouse_Left);
    lF.Frame(EUIInputMode::GameAndUI);
    CHECK(lF.Canvas.GetPressed() == lF.Button);
}
