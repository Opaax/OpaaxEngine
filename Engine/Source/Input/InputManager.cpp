#include "Input/InputManager.h"

#include "Core/String/OpaaxUtf8.h"

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

    bool InputManager::WasRepeatedThisFrame(EKeyCode InKey) const noexcept
    {
        const Uint16 lIndex = ToIndex(InKey);
        return lIndex < KEY_STATE_COUNT && m_RepeatedThisFrame[lIndex];
    }

    float InputManager::GetKeyValue(EKeyCode InKey) const noexcept
    {
        if (InKey == EKeyCode::Mouse_WheelAxis)
        {
            return m_ScrollDelta.y;
        }

        // Held, or pressed and released within this frame (a quick tap, a notch).
        return (IsKeyDown(InKey) || WasPressedThisFrame(InKey)) ? 1.f : 0.f;
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
        // A wheel is never held down.
        if (IsMouseWheel(InKey))
        {
            return;
        }

        const Uint16 lIndex = ToIndex(InKey);

        // Key repeat does not change state (WasPressedThisFrame must not fire again): it is only told
        // to text entry, for a key still held.
        if (InRepeat)
        {
            if (lIndex < KEY_STATE_COUNT && m_Current[lIndex])
            {
                m_RepeatedThisFrame[lIndex] = true;
            }
            return;
        }

        if (lIndex < KEY_STATE_COUNT)
        {
            m_Current[lIndex]          = true;
            m_PressedThisFrame[lIndex] = true;
        }
    }

    void InputManager::OnKeyReleased(EKeyCode InKey)
    {
        if (IsMouseWheel(InKey))
        {
            return;
        }

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

        if (InYOffset > 0.f) { TapThisFrame(EKeyCode::Mouse_WheelUp); }
        if (InYOffset < 0.f) { TapThisFrame(EKeyCode::Mouse_WheelDown); }
    }

    void InputManager::OnCharTyped(const Uint32 InCodepoint)
    {
        // Control characters are keys (Enter, Tab, Backspace, Delete); surrogates and values past
        // Unicode are not characters.
        const bool bControl = InCodepoint < 0x20u || (InCodepoint >= 0x7Fu && InCodepoint < 0xA0u);
        const bool bInvalid = (InCodepoint >= 0xD800u && InCodepoint <= 0xDFFFu) || InCodepoint > 0x10FFFFu;
        if (bControl || bInvalid)
        {
            return;
        }
        m_TypedText.push_back(InCodepoint);
    }

    OpaaxString InputManager::GetTypedTextUtf8() const
    {
        OpaaxString lText;
        for (const Uint32 lCodepoint : m_TypedText)
        {
            Utf8::Encode(lCodepoint, lText);
        }
        return lText;
    }

    void InputManager::TapThisFrame(EKeyCode InKey)
    {
        // Never held: the press is seen this frame, and EndFrame clears both edges.
        const Uint16 lIndex = static_cast<Uint16>(InKey);
        m_PressedThisFrame[lIndex]  = true;
        m_ReleasedThisFrame[lIndex] = true;
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
        m_RepeatedThisFrame.fill(false);
        m_TypedText.clear();

        m_ScrollDelta       = Vector2F{0.f, 0.f};
        m_MousePrevious     = m_MousePosition;
        m_bHasMousePosition = false;
    }

    void InputManager::EndFrame()
    {
        m_PressedThisFrame.fill(false);
        m_ReleasedThisFrame.fill(false);
        m_RepeatedThisFrame.fill(false);
        m_TypedText.clear();

        m_MousePrevious = m_MousePosition;
        m_ScrollDelta   = Vector2F{0.f, 0.f};
    }
}
