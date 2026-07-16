#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Events/Event.h"
#include "Core/Engine/Subsystems/Input/InputCodes.h"

namespace Opaax
{
    // =============================================================================
    // Input event payloads (POD) + Tier-1 dispatched wrappers
    //
    // Same split as the window events: the bare-noun struct (KeyPressed) is the POD
    // payload — trivially copyable, the Tier-2 delegate / Tier-3 bus currency — and
    // the <Name>Event class wraps it for Tier-1 dispatch. Flat: each event derives
    // Event directly (no shared KeyEvent/MouseButtonEvent base) and stamps its own
    // category. GetPayload() hands the POD back for bus republish.
    // =============================================================================

    // -----------------------------------------------------------------------------
    // KeyPressed — bRepeat distinguishes initial press from OS key-repeat
    struct KeyPressed
    {
        EKeyCode Code    = EKeyCode::None;
        bool     bRepeat = false;
    };

    class OPAAX_API KeyPressedEvent final : public Event
    {
    public:
        explicit KeyPressedEvent(const KeyPressed& InData) noexcept : m_Data(InData) {}
        KeyPressedEvent(EKeyCode InCode, bool InRepeat) noexcept : m_Data{InCode, InRepeat} {}

        FORCEINLINE const KeyPressed& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE EKeyCode GetKeyCode() const noexcept { return m_Data.Code; }
        FORCEINLINE bool     IsRepeat()   const noexcept { return m_Data.bRepeat; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::KeyPressed)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Input | EEventCategory::Keyboard)

    private:
        KeyPressed m_Data;
    };

    // -----------------------------------------------------------------------------
    // KeyReleased
    struct KeyReleased
    {
        EKeyCode Code = EKeyCode::None;
    };

    class OPAAX_API KeyReleasedEvent final : public Event
    {
    public:
        explicit KeyReleasedEvent(const KeyReleased& InData) noexcept : m_Data(InData) {}
        explicit KeyReleasedEvent(EKeyCode InCode) noexcept : m_Data{InCode} {}

        FORCEINLINE const KeyReleased& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE EKeyCode GetKeyCode() const noexcept { return m_Data.Code; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::KeyReleased)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Input | EEventCategory::Keyboard)

    private:
        KeyReleased m_Data;
    };

    // -----------------------------------------------------------------------------
    // KeyTyped — Unicode codepoint, for text input (NOT gameplay key detection)
    struct KeyTyped
    {
        Uint32 Codepoint = 0;
    };

    class OPAAX_API KeyTypedEvent final : public Event
    {
    public:
        explicit KeyTypedEvent(const KeyTyped& InData) noexcept : m_Data(InData) {}
        explicit KeyTypedEvent(Uint32 InCodepoint) noexcept : m_Data{InCodepoint} {}

        FORCEINLINE const KeyTyped& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE Uint32 GetCodepoint() const noexcept { return m_Data.Codepoint; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::KeyTyped)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Input | EEventCategory::Keyboard)

    private:
        KeyTyped m_Data;
    };

    // -----------------------------------------------------------------------------
    // MouseButtonPressed
    struct MouseButtonPressed
    {
        EKeyCode Button = EKeyCode::None;
    };

    class OPAAX_API MouseButtonPressedEvent final : public Event
    {
    public:
        explicit MouseButtonPressedEvent(const MouseButtonPressed& InData) noexcept : m_Data(InData) {}
        explicit MouseButtonPressedEvent(EKeyCode InButton) noexcept : m_Data{InButton} {}

        FORCEINLINE const MouseButtonPressed& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE EKeyCode GetMouseButton() const noexcept { return m_Data.Button; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseButtonPressed)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Input | EEventCategory::Mouse | EEventCategory::MouseButton)

    private:
        MouseButtonPressed m_Data;
    };

    // -----------------------------------------------------------------------------
    // MouseButtonReleased
    struct MouseButtonReleased
    {
        EKeyCode Button = EKeyCode::None;
    };

    class OPAAX_API MouseButtonReleasedEvent final : public Event
    {
    public:
        explicit MouseButtonReleasedEvent(const MouseButtonReleased& InData) noexcept : m_Data(InData) {}
        explicit MouseButtonReleasedEvent(EKeyCode InButton) noexcept : m_Data{InButton} {}

        FORCEINLINE const MouseButtonReleased& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE EKeyCode GetMouseButton() const noexcept { return m_Data.Button; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseButtonReleased)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Input | EEventCategory::Mouse | EEventCategory::MouseButton)

    private:
        MouseButtonReleased m_Data;
    };

    // -----------------------------------------------------------------------------
    // MouseMoved — cursor position, window space, pixels
    struct MouseMoved
    {
        float X = 0.f;
        float Y = 0.f;
    };

    class OPAAX_API MouseMovedEvent final : public Event
    {
    public:
        explicit MouseMovedEvent(const MouseMoved& InData) noexcept : m_Data(InData) {}
        MouseMovedEvent(float InX, float InY) noexcept : m_Data{InX, InY} {}

        FORCEINLINE const MouseMoved& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE float GetX() const noexcept { return m_Data.X; }
        FORCEINLINE float GetY() const noexcept { return m_Data.Y; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseMoved)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Input | EEventCategory::Mouse)

    private:
        MouseMoved m_Data;
    };

    // -----------------------------------------------------------------------------
    // MouseScrolled
    struct MouseScrolled
    {
        float XOffset = 0.f;
        float YOffset = 0.f;
    };

    class OPAAX_API MouseScrolledEvent final : public Event
    {
    public:
        explicit MouseScrolledEvent(const MouseScrolled& InData) noexcept : m_Data(InData) {}
        MouseScrolledEvent(float InXOffset, float InYOffset) noexcept : m_Data{InXOffset, InYOffset} {}

        FORCEINLINE const MouseScrolled& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE float GetXOffset() const noexcept { return m_Data.XOffset; }
        FORCEINLINE float GetYOffset() const noexcept { return m_Data.YOffset; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::MouseScrolled)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Input | EEventCategory::Mouse)

    private:
        MouseScrolled m_Data;
    };
}
