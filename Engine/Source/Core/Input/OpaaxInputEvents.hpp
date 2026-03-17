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

    // ==========================================================================================================
    //
    // Mouse button events.
    // Is kind of duplicated of KeyEvent since the enum keycode is the same for mouse input/Key input.
    // But I think its worth in this case.
    // Use variant in the future ? std::variant<EKeycode, EMouseCode, EGamepadCode>?
    //    
    // ==========================================================================================================

    /**
     * @class MouseButtonEvent
     *
     * Base, not dispatched directly
     */
    class OPAAX_API abstract MouseButtonEvent : public OpaaxEvent
    {
        // =============================================================================
        // CTOR 
        // =============================================================================
    protected:
        explicit MouseButtonEvent(EOpaaxKeyCode InButton) noexcept
            : m_Button(InButton) {}

        // =============================================================================
        // Functions 
        // =============================================================================
    public:
        // -----------------------------------------------------------------------------
        // Getters
        FORCEINLINE EOpaaxKeyCode GetMouseButton() const noexcept { return m_Button; }

        // =============================================================================
        // Implementation 
        // =============================================================================

        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Mouse | EEventCategory_MouseButton)

        // =============================================================================
        // Members 
        // =============================================================================
    private:
        EOpaaxKeyCode m_Button;
    };

    /**
     * @class MouseButtonPressedEvent
     */
    class OPAAX_API MouseButtonPressedEvent final : public MouseButtonEvent
    {
        // =============================================================================
        // CTOR  
        // =============================================================================
    public:
        explicit MouseButtonPressedEvent(EOpaaxKeyCode InButton) noexcept
            : MouseButtonEvent(InButton) {}

        // =============================================================================
        // Implementation 
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseButtonPressed)
    };

    /**
     * @class MouseButtonReleasedEvent
     */
    class OPAAX_API MouseButtonReleasedEvent final : public MouseButtonEvent
    {
        // =============================================================================
        // CTOR  
        // =============================================================================
    public:
        explicit MouseButtonReleasedEvent(EOpaaxKeyCode InButton) noexcept
            : MouseButtonEvent(InButton) {}

        // =============================================================================
        // Implementation 
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseButtonReleased)
    };

    /**
     * @class MouseMovedEvent
     */
    class OPAAX_API MouseMovedEvent final : public OpaaxEvent
    {
        // =============================================================================
        // CTOR  
        // =============================================================================
    public:
        MouseMovedEvent(float InX, float InY) noexcept
            : m_X(InX), m_Y(InY)
        {}

        // =============================================================================
        // Functions 
        // =============================================================================
    public:
        // -----------------------------------------------------------------------------
        // Getters
        FORCEINLINE float GetX() const noexcept { return m_X; }
        FORCEINLINE float GetY() const noexcept { return m_Y; }

        // =============================================================================
        // Implementation 
        // =============================================================================
        
        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseMoved)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Mouse)

        // =============================================================================
        // Members 
        // =============================================================================
    private:
        float m_X;
        float m_Y;
    };
 
    /**
     * @class MouseScrolledEvent
     */
    class OPAAX_API MouseScrolledEvent final : public OpaaxEvent
    {
        // =============================================================================
        // CTOR 
        // =============================================================================
    public:
        MouseScrolledEvent(float InXOffset, float InYOffset) noexcept
            : m_XOffset(InXOffset), m_YOffset(InYOffset)
        {}

        // =============================================================================
        // Functions 
        // =============================================================================
    public:
        // -----------------------------------------------------------------------------
        // Getters
        FORCEINLINE float GetXOffset() const noexcept { return m_XOffset; }
        FORCEINLINE float GetYOffset() const noexcept { return m_YOffset; }

        // =============================================================================
        // Implementation 
        // =============================================================================
        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseScrolled)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Mouse)

        // =============================================================================
        // Members 
        // =============================================================================
    private:
        float m_XOffset;
        float m_YOffset;
    };
}
