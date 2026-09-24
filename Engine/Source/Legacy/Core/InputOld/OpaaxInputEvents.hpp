#pragma once
#include "OpaaxInputTypes.hpp"
#include "Core/EventOld/OpaaxEvent.hpp"

namespace Opaax
{
    /**
     * @class KeyEventOld
     *
     * Base, not dispatched directly
     */
    class OPAAX_API KeyEventOld : public OpaaxEvent
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    protected:
        explicit KeyEventOld(EOpaaxKeyCode InKeyCode) noexcept
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
     * @class KeyPressedEventOld
     *
     * bIsRepeat distinguishes initial press from OS key-repeat.
     */
    class OPAAX_API KeyPressedEventOld final : public KeyEventOld
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        KeyPressedEventOld(EOpaaxKeyCode InKeyCode, bool InIsRepeat) noexcept
            : KeyEventOld(InKeyCode), m_bIsRepeat(InIsRepeat) {}

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

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::KeyPressed)

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool m_bIsRepeat;
    };

    /**
     * @class KeyReleasedEventOld
     */
    class OPAAX_API KeyReleasedEventOld final : public KeyEventOld
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit KeyReleasedEventOld(EOpaaxKeyCode InKeyCode) noexcept
            : KeyEventOld(InKeyCode) {}

        // =============================================================================
        // Implementation
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::KeyReleased)
    };

    /**
     * @class KeyTypedEventOld
     *
     * Carries a Unicode codepoint, not a keycode.
     * Use this for text input (chat, debug console, name entry). Do NOT use for gameplay key detection.
     */
    class OPAAX_API KeyTypedEventOld final : public OpaaxEvent
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit KeyTypedEventOld(Uint32 InCodepoint) noexcept
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

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::KeyTyped)
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
     * @class MouseButtonEventOld
     *
     * Base, not dispatched directly
     */
    class OPAAX_API MouseButtonEventOld : public OpaaxEvent
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    protected:
        explicit MouseButtonEventOld(EOpaaxKeyCode InButton) noexcept
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
     * @class MouseButtonPressedEventOld
     */
    class OPAAX_API MouseButtonPressedEventOld final : public MouseButtonEventOld
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit MouseButtonPressedEventOld(EOpaaxKeyCode InButton) noexcept
            : MouseButtonEventOld(InButton) {}

        // =============================================================================
        // Implementation
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::MouseButtonPressed)
    };

    /**
     * @class MouseButtonReleasedEventOld
     */
    class OPAAX_API MouseButtonReleasedEventOld final : public MouseButtonEventOld
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit MouseButtonReleasedEventOld(EOpaaxKeyCode InButton) noexcept
            : MouseButtonEventOld(InButton) {}

        // =============================================================================
        // Implementation
        // =============================================================================

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::MouseButtonReleased)
    };

    /**
     * @class MouseMovedEventOld
     */
    class OPAAX_API MouseMovedEventOld final : public OpaaxEvent
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        MouseMovedEventOld(float InX, float InY) noexcept
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

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::MouseMoved)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Mouse)

        // =============================================================================
        // Members
        // =============================================================================
    private:
        float m_X;
        float m_Y;
    };

    /**
     * @class MouseScrolledEventOld
     */
    class OPAAX_API MouseScrolledEventOld final : public OpaaxEvent
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        MouseScrolledEventOld(float InXOffset, float InYOffset) noexcept
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
        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::MouseScrolled)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Mouse)

        // =============================================================================
        // Members
        // =============================================================================
    private:
        float m_XOffset;
        float m_YOffset;
    };
}
