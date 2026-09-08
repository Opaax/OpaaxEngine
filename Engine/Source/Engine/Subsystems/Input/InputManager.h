#pragma once

#include <array>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Engine/Subsystems/Input/InputCodes.h"

// =============================================================================
// InputManager
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogInputManager{"InputManager"};

    // =============================================================================
    // InputManager — WHAT IS CURRENTLY HELD, and what changed this frame.
    //
    //   The engine end of the input chain: Window -> Application -> route -> here (Editor.md D5).
    //   The application FEEDS it (OnKeyPressed & co.) as OS events arrive; everything else only
    //   ever reads it. It knows about physical keys and nothing about meaning — action maps are a
    //   game-layer concept built on top (D5), so there is no "Jump" in here.
    //
    //   WHERE THERE IS ONE: Engine/Input/InputMappingSubsystem. It reads this every frame and
    //   turns it into named actions, and it is a GAME-INSTANCE subsystem rather than an engine one
    //   because a mapping context must outlive any single world (GI1). Nothing here knows about it.
    //
    //   FEEDING IS IMMEDIATE, not queued. Events are applied during PollEvents, before the frame
    //   ticks, so a reader mid-route (the editor asking "is Shift held?" while handling a key) gets
    //   the truth rather than last frame's copy.
    //
    //   NO TICK HOOK, deliberately. Edge queries need a snapshot taken between one frame's end and
    //   the next frame's OS events — and PollEvents runs BEFORE Engine::Loop, so a subsystem
    //   Update() would run after the very events it is meant to precede. Engine::Loop calls
    //   EndFrame() explicitly, after Render, which is that moment.
    //
    //   Keyboard and mouse only. GLFW exposes gamepads by POLLING rather than callbacks — a second
    //   feed with its own semantics — so those codes are refused here rather than half-supported.
    // =============================================================================
    class OPAAX_API InputManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(InputManager)

        /**
         * Every keyboard and mouse code fits below this (Mouse_Button8 = 507); gamepad starts at
         * 10000. A dense array over this range is ~1 KB and makes every query an index.
         */
        static constexpr Uint16 KEY_STATE_COUNT = 512;

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        InputManager()           = default;
        ~InputManager() override = default;

        // =============================================================================
        // Query — held state
        // =============================================================================
    public:
        /** @return true while InKey is held, every frame between its press and its release. */
        bool IsKeyDown(EKeyCode InKey) const noexcept;

        /**
         * @return true ONLY on the frame InKey went down. False on OS key-repeat.
         *
         * LATCHED BY THE FEED, not derived from a previous-frame snapshot. A tap that starts and
         * ends inside one frame leaves both snapshots reading "up", so a comparison would drop it
         * entirely — and a frame is long enough for a real keypress the moment the framerate dips.
         * The latch records that it HAPPENED; EndFrame clears it.
         */
        bool WasPressedThisFrame(EKeyCode InKey) const noexcept;

        /** @return true ONLY on the frame InKey came up. Never fires for a ResetState release. */
        bool WasReleasedThisFrame(EKeyCode InKey) const noexcept;

        /**
         * Modifiers are just KEYS — there is no separate modifier state, which is why nothing needs
         * the GLFW `mods` parameter the window callback discards. Left or right, either counts.
         */
        bool IsShiftDown() const noexcept;
        bool IsCtrlDown()  const noexcept;
        bool IsAltDown()   const noexcept;

        /** Every key currently held, in code order. For the editor's Input panel. */
        TDynArray<EKeyCode> GetKeysDown() const;

        // =============================================================================
        // Query — mouse
        // =============================================================================
    public:
        /** Cursor position in WINDOW PIXELS. World space needs the viewport rect and the camera. */
        Vector2F GetMousePosition() const noexcept { return m_MousePosition; }

        /** Movement since the previous frame, in window pixels. Zero when the mouse did not move. */
        Vector2F GetMouseDelta() const noexcept;

        /** Wheel movement accumulated THIS frame; cleared by EndFrame. */
        Vector2F GetScrollDelta() const noexcept { return m_ScrollDelta; }

        // =============================================================================
        // Feed — called by the application as OS events arrive
        // =============================================================================
    public:
        /** @param InRepeat OS key-repeat. Held state is unchanged and no press-edge is produced. */
        void OnKeyPressed(EKeyCode InKey, bool InRepeat);
        void OnKeyReleased(EKeyCode InKey);

        void OnMouseButtonPressed(EKeyCode InButton);
        void OnMouseButtonReleased(EKeyCode InButton);

        void OnMouseMoved(float InX, float InY);
        void OnMouseScrolled(float InXOffset, float InYOffset);

        // =============================================================================
        // Frame + route control
        // =============================================================================
    public:
        /**
         * Release everything and zero the deltas — "the route closed" (D5). Called on window focus
         * loss, and by the editor whenever it stops feeding the engine.
         *
         * Without it a key held when the route closes stays held forever: the OS delivers the
         * release to whoever has focus, and that is no longer us.
         *
         * The edge latches are CLEARED rather than filled in, so nothing reports
         * WasReleasedThisFrame for a press the reader may never have seen — a reset is not an
         * event, it is an admission that the state is unknowable.
         */
        void ResetState();

        /**
         * Close the frame: edge latches clear, mouse delta rebases, scroll clears.
         *
         * Called by Engine::Loop AFTER Render — the only point between this frame's end and the
         * next frame's PollEvents. See the class note on why this is not a tick hook.
         */
        void EndFrame();

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup() override;
        void Shutdown() override;
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * @return The index for InKey, or KEY_STATE_COUNT when it is out of range (a gamepad code,
         *   or None). Warns ONCE per run: a repeat would spam every frame the pad is touched.
         */
        Uint16 ToIndex(EKeyCode InKey) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Held state, plus two edge latches set by the feed and cleared by EndFrame. A
        // previous-frame snapshot would be smaller by one array and WRONG: it cannot represent a
        // key that went down and up between two reads, which is any tap during a slow frame.
        std::array<bool, KEY_STATE_COUNT> m_Current{};
        std::array<bool, KEY_STATE_COUNT> m_PressedThisFrame{};
        std::array<bool, KEY_STATE_COUNT> m_ReleasedThisFrame{};

        Vector2F m_MousePosition{0.f, 0.f};
        Vector2F m_MousePrevious{0.f, 0.f};
        Vector2F m_ScrollDelta{0.f, 0.f};

        // The first mouse event carries an absolute position with no predecessor; without this the
        // opening frame reports a delta the size of the cursor's distance from the origin.
        bool m_bHasMousePosition = false;

        mutable bool m_bWarnedOutOfRange = false;

        // One-shot proof-of-life for the feed; see OnKeyPressed. NOT cleared by ResetState — it is
        // about the run, not about the route.
        bool m_bLoggedFirstKey = false;
    };
}
