// Suite: InputManager — held state, per-frame edges, and the reset when input stops being fed.
//   Every test feeds events then calls EndFrame, the order the application uses.
#include <doctest.h>

#include "Engine/Subsystems/Input/InputCodes.h"
#include "Engine/Subsystems/Input/InputManager.h"

using namespace Opaax;

// =============================================================================
// Held state
// =============================================================================
TEST_CASE("input: a key reads as down from its press until its release")
{
    InputManager lInput;

    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::W));

    lInput.OnKeyPressed(EKeyCode::W, false);
    CHECK(lInput.IsKeyDown(EKeyCode::W));

    // Still held across frames — the whole point of state rather than events.
    lInput.EndFrame();
    lInput.EndFrame();
    CHECK(lInput.IsKeyDown(EKeyCode::W));

    lInput.OnKeyReleased(EKeyCode::W);
    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::W));
}

TEST_CASE("input: keys are independent, and mouse buttons are just keys")
{
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::A, false);
    lInput.OnMouseButtonPressed(EKeyCode::Mouse_Left);

    CHECK(lInput.IsKeyDown(EKeyCode::A));
    CHECK(lInput.IsKeyDown(EKeyCode::Mouse_Left));
    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::D));

    lInput.OnKeyReleased(EKeyCode::A);
    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::A));
    CHECK(lInput.IsKeyDown(EKeyCode::Mouse_Left));   // untouched by A's release
}

TEST_CASE("input: modifiers are KEYS — left or right, no separate state")
{
    // This is why nothing needs the GLFW `mods` parameter the window callback discards.
    InputManager lInput;

    CHECK_FALSE(lInput.IsShiftDown());

    lInput.OnKeyPressed(EKeyCode::RightShift, false);
    CHECK(lInput.IsShiftDown());
    CHECK_FALSE(lInput.IsCtrlDown());

    lInput.OnKeyPressed(EKeyCode::LeftControl, false);
    CHECK(lInput.IsCtrlDown());

    lInput.OnKeyReleased(EKeyCode::RightShift);
    CHECK_FALSE(lInput.IsShiftDown());
    CHECK(lInput.IsCtrlDown());
}

// =============================================================================
// Edges — the part that depends on WHEN
// =============================================================================
TEST_CASE("input: a press-edge fires on ONE frame only")
{
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::Space, false);

    CHECK(lInput.WasPressedThisFrame(EKeyCode::Space));
    CHECK(lInput.IsKeyDown(EKeyCode::Space));

    // Next frame: still held, no longer a press. A jump that fired every frame of the hold is
    // exactly the bug this separation exists to prevent.
    lInput.EndFrame();
    CHECK_FALSE(lInput.WasPressedThisFrame(EKeyCode::Space));
    CHECK(lInput.IsKeyDown(EKeyCode::Space));
}

TEST_CASE("input: a release-edge fires on ONE frame only")
{
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::Space, false);
    lInput.EndFrame();
    CHECK_FALSE(lInput.WasReleasedThisFrame(EKeyCode::Space));   // still held

    lInput.OnKeyReleased(EKeyCode::Space);
    CHECK(lInput.WasReleasedThisFrame(EKeyCode::Space));
    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::Space));

    lInput.EndFrame();
    CHECK_FALSE(lInput.WasReleasedThisFrame(EKeyCode::Space));
}

TEST_CASE("input: OS key-repeat does NOT re-fire the press edge")
{
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::D, false);
    lInput.EndFrame();
    REQUIRE_FALSE(lInput.WasPressedThisFrame(EKeyCode::D));

    // Holding a key makes the OS resend presses. That is a text-entry concept; the key never
    // came up, so there is no new edge.
    lInput.OnKeyPressed(EKeyCode::D, true);
    CHECK_FALSE(lInput.WasPressedThisFrame(EKeyCode::D));
    CHECK(lInput.IsKeyDown(EKeyCode::D));
}

TEST_CASE("input: a tap that starts AND ends inside one frame is not lost")
{
    // A key pressed and released between two reads must still be seen. Edges are latched by the
    // feed rather than derived from snapshots (both snapshots would read "up").
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::E, false);
    lInput.OnKeyReleased(EKeyCode::E);

    CHECK(lInput.WasPressedThisFrame(EKeyCode::E));
    CHECK(lInput.WasReleasedThisFrame(EKeyCode::E));
    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::E));   // correctly not held: it came back up

    // Both latches clear together — the tap is reported once, not forever.
    lInput.EndFrame();
    CHECK_FALSE(lInput.WasPressedThisFrame(EKeyCode::E));
    CHECK_FALSE(lInput.WasReleasedThisFrame(EKeyCode::E));
}

// =============================================================================
// Mouse
// =============================================================================
TEST_CASE("input: mouse delta is per-frame, and the FIRST position produces no delta")
{
    InputManager lInput;

    // Without the first-position guard this would report a jump the size of the cursor's
    // distance from the origin, on frame one, every run.
    lInput.OnMouseMoved(800.f, 400.f);
    CHECK(lInput.GetMousePosition().x == doctest::Approx(800.f));
    CHECK(lInput.GetMouseDelta().x == doctest::Approx(0.f));
    CHECK(lInput.GetMouseDelta().y == doctest::Approx(0.f));

    lInput.EndFrame();

    lInput.OnMouseMoved(790.f, 411.f);
    CHECK(lInput.GetMouseDelta().x == doctest::Approx(-10.f));
    CHECK(lInput.GetMouseDelta().y == doctest::Approx(11.f));

    // No movement next frame => no delta, rather than the previous one lingering.
    lInput.EndFrame();
    CHECK(lInput.GetMouseDelta().x == doctest::Approx(0.f));
    CHECK(lInput.GetMousePosition().x == doctest::Approx(790.f));
}

TEST_CASE("input: scroll ACCUMULATES within a frame and clears at EndFrame")
{
    InputManager lInput;

    // Several notches can land in one PollEvents; keeping only the last would drop them.
    lInput.OnMouseScrolled(0.f, 1.f);
    lInput.OnMouseScrolled(0.f, 2.f);
    CHECK(lInput.GetScrollDelta().y == doctest::Approx(3.f));

    lInput.EndFrame();
    CHECK(lInput.GetScrollDelta().y == doctest::Approx(0.f));
}

// =============================================================================
// ResetState — the route-close contract
// =============================================================================
TEST_CASE("input: ResetState releases everything held")
{
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::W, false);
    lInput.OnKeyPressed(EKeyCode::LeftShift, false);
    lInput.OnMouseButtonPressed(EKeyCode::Mouse_Left);
    lInput.OnMouseScrolled(0.f, 5.f);
    lInput.EndFrame();

    REQUIRE(lInput.IsKeyDown(EKeyCode::W));

    lInput.ResetState();

    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::W));
    CHECK_FALSE(lInput.IsShiftDown());
    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::Mouse_Left));
    CHECK(lInput.GetScrollDelta().y == doctest::Approx(0.f));
    CHECK(lInput.GetKeysDown().empty());
}

TEST_CASE("input: ResetState leaves NO phantom release-edge")
{
    // A reset is not a release: a key held at reset must not report WasReleasedThisFrame.
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::W, false);
    lInput.EndFrame();
    REQUIRE(lInput.IsKeyDown(EKeyCode::W));

    lInput.ResetState();

    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::W));
    CHECK_FALSE(lInput.WasReleasedThisFrame(EKeyCode::W));
    CHECK_FALSE(lInput.WasPressedThisFrame(EKeyCode::W));

    // And the state is genuinely fresh afterwards: a new press still edges normally.
    lInput.OnKeyPressed(EKeyCode::W, false);
    CHECK(lInput.WasPressedThisFrame(EKeyCode::W));
}

TEST_CASE("input: a re-press after ResetState behaves like a first press")
{
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::A, false);
    lInput.EndFrame();
    lInput.ResetState();

    // The key is physically still held, but the engine has stopped being told about it. When the
    // route reopens, the next press is the first thing it knows — and must edge.
    lInput.OnKeyPressed(EKeyCode::A, false);
    CHECK(lInput.WasPressedThisFrame(EKeyCode::A));
    CHECK(lInput.IsKeyDown(EKeyCode::A));
}

TEST_CASE("input: mouse delta does not jump across a ResetState")
{
    InputManager lInput;

    lInput.OnMouseMoved(100.f, 100.f);
    lInput.EndFrame();
    lInput.OnMouseMoved(120.f, 100.f);
    REQUIRE(lInput.GetMouseDelta().x == doctest::Approx(20.f));

    lInput.ResetState();

    // The cursor may have travelled anywhere while the route was closed; the first position after
    // it reopens is a fresh origin, not a delta of however far the mouse went unobserved.
    lInput.OnMouseMoved(900.f, 700.f);
    CHECK(lInput.GetMouseDelta().x == doctest::Approx(0.f));
    CHECK(lInput.GetMouseDelta().y == doctest::Approx(0.f));
}

// =============================================================================
// Range
// =============================================================================
TEST_CASE("input: gamepad codes are refused rather than half-supported")
{
    InputManager lInput;

    // Gamepad codes are reserved but have no feed yet: refuse them.
    lInput.OnKeyPressed(EKeyCode::Gamepad_FaceButton_Bottom, false);

    CHECK_FALSE(lInput.IsKeyDown(EKeyCode::Gamepad_FaceButton_Bottom));
    CHECK(lInput.GetKeysDown().empty());

    // None is not a key either.
    lInput.OnKeyPressed(EKeyCode::None, false);
    CHECK(lInput.GetKeysDown().empty());
}

TEST_CASE("input: GetKeysDown reports exactly what is held, in code order")
{
    InputManager lInput;

    lInput.OnKeyPressed(EKeyCode::W, false);            // 87
    lInput.OnKeyPressed(EKeyCode::LeftShift, false);    // 340
    lInput.OnMouseButtonPressed(EKeyCode::Mouse_Left);  // 500

    const TDynArray<EKeyCode> lDown = lInput.GetKeysDown();

    REQUIRE(lDown.size() == 3u);
    CHECK(lDown[0] == EKeyCode::W);
    CHECK(lDown[1] == EKeyCode::LeftShift);
    CHECK(lDown[2] == EKeyCode::Mouse_Left);
}
