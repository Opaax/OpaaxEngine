#include "Engine/Subsystems/Input/InputManager.h"

namespace Opaax
{
    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool InputManager::Startup()
    {
        ResetState();

        OPAAX_LOG(LogInputManager, Info, "InputManager started (keyboard + mouse; fed by the application)")
        return true;
    }

    void InputManager::Shutdown()
    {
        OPAAX_LOG(LogInputManager, Info, "InputManager shutdown")
    }

    // =========================================================================
    // Query
    // =========================================================================
    Uint16 InputManager::ToIndex(EKeyCode InKey) const
    {
        const Uint16 lRaw = static_cast<Uint16>(InKey);

        if (lRaw == 0 || lRaw >= KEY_STATE_COUNT)
        {
            if (!m_bWarnedOutOfRange)
            {
                m_bWarnedOutOfRange = true;
                OPAAX_LOG(LogInputManager, Warn,
                          "Key code {} is outside the keyboard/mouse range — ignored (gamepad has no feed yet). Warned once.",
                          lRaw)
            }

            return KEY_STATE_COUNT;
        }

        return lRaw;
    }

    bool InputManager::IsKeyDown(EKeyCode InKey) const noexcept
    {
        const Uint16 lIndex = ToIndex(InKey);
        return lIndex < KEY_STATE_COUNT && m_Current[lIndex];
    }

    bool InputManager::WasPressedThisFrame(EKeyCode InKey) const noexcept
    {
        const Uint16 lIndex = ToIndex(InKey);
        return lIndex < KEY_STATE_COUNT && m_PressedThisFrame[lIndex];
    }

    bool InputManager::WasReleasedThisFrame(EKeyCode InKey) const noexcept
    {
        const Uint16 lIndex = ToIndex(InKey);
        return lIndex < KEY_STATE_COUNT && m_ReleasedThisFrame[lIndex];
    }

    bool InputManager::IsShiftDown() const noexcept
    {
        return IsKeyDown(EKeyCode::LeftShift) || IsKeyDown(EKeyCode::RightShift);
    }

    bool InputManager::IsCtrlDown() const noexcept
    {
        return IsKeyDown(EKeyCode::LeftControl) || IsKeyDown(EKeyCode::RightControl);
    }

    bool InputManager::IsAltDown() const noexcept
    {
        return IsKeyDown(EKeyCode::LeftAlt) || IsKeyDown(EKeyCode::RightAlt);
    }

    TDynArray<EKeyCode> InputManager::GetKeysDown() const
    {
        TDynArray<EKeyCode> lKeys;

        for (Uint16 lIndex = 1; lIndex < KEY_STATE_COUNT; ++lIndex)
        {
            if (m_Current[lIndex])
            {
                lKeys.push_back(static_cast<EKeyCode>(lIndex));
            }
        }

        return lKeys;
    }

    Vector2F InputManager::GetMouseDelta() const noexcept
    {
        return Vector2F{m_MousePosition.x - m_MousePrevious.x, m_MousePosition.y - m_MousePrevious.y};
    }

    // =========================================================================
    // Feed
    // =========================================================================
    void InputManager::OnKeyPressed(EKeyCode InKey, bool InRepeat)
    {
        if (InRepeat)
        {
            // OS key-repeat is a text-entry concept, not a state change: the key was already down,
            // and letting it through would make WasPressedThisFrame fire again mid-hold.
            return;
        }

        const Uint16 lIndex = ToIndex(InKey);
        if (lIndex < KEY_STATE_COUNT)
        {
            m_Current[lIndex]          = true;
            m_PressedThisFrame[lIndex] = true;

            // Once per run: proof the whole chain reached here — window -> application -> route ->
            // engine. Everything else about input is silent by necessity (a per-event log would
            // spam every frame), so without this line a host that feeds nothing at all looks
            // exactly like one that works (L15).
            if (!m_bLoggedFirstKey)
            {
                m_bLoggedFirstKey = true;
                OPAAX_LOG(LogInputManager, Info, "First key reached the engine (code {}) — the input chain is live", lIndex)
            }
        }
    }

    void InputManager::OnKeyReleased(EKeyCode InKey)
    {
        const Uint16 lIndex = ToIndex(InKey);
        if (lIndex < KEY_STATE_COUNT)
        {
            m_Current[lIndex]           = false;
            m_ReleasedThisFrame[lIndex] = true;
        }
    }

    void InputManager::OnMouseButtonPressed(EKeyCode InButton)
    {
        OnKeyPressed(InButton, false);
    }

    void InputManager::OnMouseButtonReleased(EKeyCode InButton)
    {
        OnKeyReleased(InButton);
    }

    void InputManager::OnMouseMoved(float InX, float InY)
    {
        m_MousePosition = Vector2F{InX, InY};

        if (!m_bHasMousePosition)
        {
            // First position of the run (or since a reset): there is no previous, so the delta must
            // be zero rather than the cursor's distance from the origin.
            m_MousePrevious      = m_MousePosition;
            m_bHasMousePosition  = true;
        }
    }

    void InputManager::OnMouseScrolled(float InXOffset, float InYOffset)
    {
        // Accumulated, not assigned: several wheel notches can land in one frame's PollEvents, and
        // keeping only the last would silently drop them.
        m_ScrollDelta.x += InXOffset;
        m_ScrollDelta.y += InYOffset;
    }

    // =========================================================================
    // Frame + route control
    // =========================================================================
    void InputManager::ResetState()
    {
        m_Current.fill(false);

        // The latches are CLEARED, not filled: a reset must not produce release-edges for presses
        // the reader may never have observed. See the header — a reset is not an event.
        m_PressedThisFrame.fill(false);
        m_ReleasedThisFrame.fill(false);

        m_ScrollDelta       = Vector2F{0.f, 0.f};
        m_MousePrevious     = m_MousePosition;
        m_bHasMousePosition = false;
    }

    void InputManager::EndFrame()
    {
        m_PressedThisFrame.fill(false);
        m_ReleasedThisFrame.fill(false);

        m_MousePrevious = m_MousePosition;
        m_ScrollDelta   = Vector2F{0.f, 0.f};
    }
}
