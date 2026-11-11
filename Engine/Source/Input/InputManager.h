#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Input/InputCodes.h"

// =============================================================================
// InputManager
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogInputManager{"InputManager"};

    // =============================================================================
    // InputManager — which keys are held, and what changed this frame.
    //   Fed by the application as OS events arrive; everything else only reads it.
    //   Physical keys only; named actions are in InputMappingSubsystem.
    //   Keyboard and mouse only (gamepad codes are refused).
    //   The wheel is never held: a notch presses and releases Mouse_WheelUp or Mouse_WheelDown in its
    //   frame, and Mouse_WheelAxis reads the notches of the frame.
    //   Text entry is apart from the keys: the characters typed this frame (GetTypedText), and the
    //   OS repeat of a held key (WasRepeatedThisFrame) for a held Backspace or arrow.
    // =============================================================================
    class InputManager final : public EngineSubsystem
    {
        OPAAX_OBJECT(InputManager, EngineSubsystem)

        /**
         * Every keyboard and mouse code is below this (gamepad starts at 10000).
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
        /** @return True while InKey is held */
        bool IsKeyDown(EKeyCode InKey) const noexcept;

        /**
         * @return True only on the frame InKey went down (not on key repeat).
         *   Also true for a tap that started and ended within the frame.
         */
        bool WasPressedThisFrame(EKeyCode InKey) const noexcept;

        /** @return True only on the frame InKey went up. Not after ResetState. */
        bool WasReleasedThisFrame(EKeyCode InKey) const noexcept;

        /**
         * @return True on a frame the OS repeated InKey while held (text entry: a held Backspace
         *   deletes again). Not on the press itself: a line edit reads press or repeat.
         */
        bool WasRepeatedThisFrame(EKeyCode InKey) const noexcept;

        /**
         * What InKey drives an action with this frame: 1 while held or tapped, else 0; for
         * Mouse_WheelAxis, the notches turned (up positive, a fraction from a touchpad).
         */
        float GetKeyValue(EKeyCode InKey) const noexcept;

        /**
         * Left or right modifier key.
         */
        bool IsShiftDown() const noexcept;
        bool IsCtrlDown()  const noexcept;
        bool IsAltDown()   const noexcept;

        /** Every held key, in code order (editor Input panel). */
        TDynArray<EKeyCode> GetKeysDown() const;

        // =============================================================================
        // Query — mouse
        // =============================================================================
    public:
        /** Cursor position in window pixels. */
        Vector2F GetMousePosition() const noexcept { return m_MousePosition; }

        /** Movement since the previous frame, in window pixels. */
        Vector2F GetMouseDelta() const noexcept;

        /** Wheel movement this frame, in notches: y up positive, x from a tilting wheel or a touchpad. */
        Vector2F GetScrollDelta() const noexcept { return m_ScrollDelta; }

        // =============================================================================
        // Query — text
        // =============================================================================
    public:
        /**
         * The characters typed this frame, as Unicode codepoints in order: what the OS made of the
         * keys (layout, Shift, dead keys). Control characters are not in it (Enter, Backspace and
         * Tab are keys). Cleared by EndFrame.
         */
        const TDynArray<Uint32>& GetTypedText() const noexcept { return m_TypedText; }

        /** GetTypedText as UTF-8. */
        OpaaxString GetTypedTextUtf8() const;

        // =============================================================================
        // Feed — called by the application as OS events arrive
        // =============================================================================
    public:
        /**
         * @param InRepeat OS key repeat: no state change, no press.
         * The wheel codes are ignored: only OnMouseScrolled turns the wheel.
         */
        void OnKeyPressed(EKeyCode InKey, bool InRepeat);
        void OnKeyReleased(EKeyCode InKey);

        void OnMouseButtonPressed(EKeyCode InButton);
        void OnMouseButtonReleased(EKeyCode InButton);

        void OnMouseMoved(float InX, float InY);

        /** Adds to this frame's scroll; a notch up or down also taps Mouse_WheelUp or Mouse_WheelDown. */
        void OnMouseScrolled(float InXOffset, float InYOffset);

        /** A character typed (the OS text input). Control characters and invalid codepoints are dropped. */
        void OnCharTyped(Uint32 InCodepoint);

        // =============================================================================
        // Frame + route control
        // =============================================================================
    public:
        /**
         * Releases everything and zeroes the deltas. Called on focus loss, and by the editor
         * when it stops feeding the engine. Produces no release events.
         */
        void ResetState();

        /**
         * Ends the frame: clears edge flags and scroll, resets the mouse delta.
         * Called by the application at the start of each loop iteration, before new events arrive.
         */
        void EndFrame();

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Subsystem interface
    public:
        bool Startup() override;
        void Shutdown() override;
        //~End Subsystem interface

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * @return The index for InKey, or KEY_STATE_COUNT when out of range. Warns once.
         */
        Uint16 ToIndex(EKeyCode InKey) const;

        /** Presses and releases InKey within this frame, as a notch does. */
        void TapThisFrame(EKeyCode InKey);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Held state, plus press/release flags set by the feed and cleared by EndFrame
        // (so a tap within one frame is not lost).
        TFixedArray<bool, KEY_STATE_COUNT> m_Current{};
        TFixedArray<bool, KEY_STATE_COUNT> m_PressedThisFrame{};
        TFixedArray<bool, KEY_STATE_COUNT> m_ReleasedThisFrame{};
        TFixedArray<bool, KEY_STATE_COUNT> m_RepeatedThisFrame{};

        TDynArray<Uint32> m_TypedText;

        Vector2F m_MousePosition{0.f, 0.f};
        Vector2F m_MousePrevious{0.f, 0.f};
        Vector2F m_ScrollDelta{0.f, 0.f};

        // The first mouse event has no previous position: no delta.
        bool m_bHasMousePosition = false;

        mutable bool m_bWarnedOutOfRange = false;
    };
}
