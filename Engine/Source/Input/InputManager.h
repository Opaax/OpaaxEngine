#pragma once

#include <array>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
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
    // =============================================================================
    class InputManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(InputManager)

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

        /** Wheel movement this frame. */
        Vector2F GetScrollDelta() const noexcept { return m_ScrollDelta; }

        // =============================================================================
        // Feed — called by the application as OS events arrive
        // =============================================================================
    public:
        /** @param InRepeat OS key repeat: no state change, no press. */
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
         * @return The index for InKey, or KEY_STATE_COUNT when out of range. Warns once.
         */
        Uint16 ToIndex(EKeyCode InKey) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Held state, plus press/release flags set by the feed and cleared by EndFrame
        // (so a tap within one frame is not lost).
        std::array<bool, KEY_STATE_COUNT> m_Current{};
        std::array<bool, KEY_STATE_COUNT> m_PressedThisFrame{};
        std::array<bool, KEY_STATE_COUNT> m_ReleasedThisFrame{};

        Vector2F m_MousePosition{0.f, 0.f};
        Vector2F m_MousePrevious{0.f, 0.f};
        Vector2F m_ScrollDelta{0.f, 0.f};

        // The first mouse event has no previous position: no delta.
        bool m_bHasMousePosition = false;

        mutable bool m_bWarnedOutOfRange = false;
    };
}
