#include "Engine/Subsystems/Input/InputManager.h"

namespace Opaax
{
    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool InputManager::Startup()
    {
        ResetState();

        return true;
    }

    void InputManager::Shutdown()
    {
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
                          lRaw);
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
                lKeys.emplace_back(static_cast<EKeyCode>(lIndex));
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
            // Key repeat does not change state (WasPressedThisFrame must not fire again).
            return;
        }

        const Uint16 lIndex = ToIndex(InKey);
        if (lIndex < KEY_STATE_COUNT)
        {
            m_Current[lIndex]          = true;
            m_PressedThisFrame[lIndex] = true;
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
            // First position: no delta.
            m_MousePrevious      = m_MousePosition;
            m_bHasMousePosition  = true;
        }
    }

    void InputManager::OnMouseScrolled(float InXOffset, float InYOffset)
    {
        // Accumulated: several wheel events can arrive in one frame.
        m_ScrollDelta.x += InXOffset;
        m_ScrollDelta.y += InYOffset;
    }

    // =========================================================================
    // Frame + route control
    // =========================================================================
    void InputManager::ResetState()
    {
        m_Current.fill(false);

        // Edge flags are cleared, not set: a reset is not a release.
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
