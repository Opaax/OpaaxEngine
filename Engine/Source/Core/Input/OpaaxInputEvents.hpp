#pragma once
#include "OpaaxInputTypes.hpp"
#include "Core/Event/OpaaxEvent.hpp"

namespace Opaax
{
    /**
     * @class KeyEvent
     *
     * Base, not dispatched directly
     */
    class OPAAX_API abstract KeyEvent : public OpaaxEvent
    {
        // =============================================================================
        // CTOR 
        // =============================================================================
    protected:
        explicit KeyEvent(EOpaaxKeyCode InKeyCode) noexcept
            : m_KeyCode(InKeyCode) {}

        // =============================================================================
        // Functions 
        // =============================================================================
    public:
        // -----------------------------------------------------------------------------
        // Getters
        FORCEINLINE EOpaaxKeyCode GetKeyCode() const noexcept { return m_KeyCode; }

        // =============================================================================
        // Implementation 
        // =============================================================================
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Keyboard)

        // =============================================================================
        // Members 
        // =============================================================================
    private:
        EOpaaxKeyCode m_KeyCode;
    };

    /**
     * @class KeyPressedEvent
     *
     * bIsRepeat distinguishes initial press from OS key-repeat.
     */
    class OPAAX_API KeyPressedEvent final : public KeyEvent
    {
        // =============================================================================
        // CTOR 
        // =============================================================================
    public:
        KeyPressedEvent(EOpaaxKeyCode InKeyCode, bool InIsRepeat) noexcept
            : KeyEvent(InKeyCode), m_bIsRepeat(InIsRepeat) {}

        // =============================================================================
        // Functions 
        // =============================================================================
    public:
        // -----------------------------------------------------------------------------
        // Getters
        FORCEINLINE bool IsRepeat() const noexcept { return m_bIsRepeat; }

        // =============================================================================
        // Implementation 
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventType::KeyPressed)

        // =============================================================================
        // Members 
        // =============================================================================
    private:
        bool m_bIsRepeat;
    };

    /**
     * @class KeyReleasedEvent
     */
    class OPAAX_API KeyReleasedEvent final : public KeyEvent
    {
        // =============================================================================
        // CTOR 
        // =============================================================================
    public:
        explicit KeyReleasedEvent(EOpaaxKeyCode InKeyCode) noexcept
            : KeyEvent(InKeyCode) {}

        // =============================================================================
        // Implementation 
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventType::KeyReleased)
    };

    /**
     * @class KeyTypedEvent
     *
     * Carries a Unicode codepoint, not a keycode.
     * Use this for text input (chat, debug console, name entry). Do NOT use for gameplay key detection.
     */
    class OPAAX_API KeyTypedEvent final : public OpaaxEvent
    {
        // =============================================================================
        // CTOR 
        // =============================================================================
    public:
        explicit KeyTypedEvent(Uint32 InCodepoint) noexcept
            : m_Codepoint(InCodepoint) {}

        // =============================================================================
        // Functions 
        // =============================================================================
    public:
        // -----------------------------------------------------------------------------
        // Getters
        FORCEINLINE Uint32 GetCodepoint() const noexcept { return m_Codepoint; }

        // =============================================================================
        // Implementation 
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventType::KeyTyped)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Keyboard)

    private:
        Uint32 m_Codepoint;
    };
}
