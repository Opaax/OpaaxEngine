#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Events/Event.h"

namespace Opaax
{
    // =============================================================================
    // Window event payloads (POD) + Tier-1 dispatched wrappers
    //
    // Convention: the bare-noun struct (WindowResize) is the POD payload — the
    // Tier-2 delegate / Tier-3 bus currency, trivially copyable. The <Name>Event
    // class wraps it for Tier-1 dispatch (adds identity + bHandled). One shape,
    // defined once; GetPayload() hands the POD back for republish on the bus.
    // =============================================================================

    // -----------------------------------------------------------------------------
    // WindowClose
    struct WindowClose {};

    class OPAAX_API WindowCloseEvent final : public Event
    {
    public:
        WindowCloseEvent() noexcept = default;

        FORCEINLINE WindowClose GetPayload() const noexcept { return {}; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowClose)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Application)
    };

    // -----------------------------------------------------------------------------
    // WindowResize
    struct WindowResize
    {
        Uint32 Width  = 0;
        Uint32 Height = 0;
    };

    class OPAAX_API WindowResizeEvent final : public Event
    {
    public:
        explicit WindowResizeEvent(const WindowResize& InData) noexcept : m_Data(InData) {}
        WindowResizeEvent(Uint32 InWidth, Uint32 InHeight) noexcept : m_Data{InWidth, InHeight} {}

        FORCEINLINE const WindowResize& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE Uint32 GetWidth()  const noexcept { return m_Data.Width; }
        FORCEINLINE Uint32 GetHeight() const noexcept { return m_Data.Height; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowResize)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Application)

    private:
        WindowResize m_Data;
    };

    // -----------------------------------------------------------------------------
    // WindowFocus
    struct WindowFocus {};

    class OPAAX_API WindowFocusEvent final : public Event
    {
    public:
        WindowFocusEvent() noexcept = default;

        FORCEINLINE WindowFocus GetPayload() const noexcept { return {}; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowFocus)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Application)
    };

    // -----------------------------------------------------------------------------
    // WindowLostFocus
    struct WindowLostFocus {};

    class OPAAX_API WindowLostFocusEvent final : public Event
    {
    public:
        WindowLostFocusEvent() noexcept = default;

        FORCEINLINE WindowLostFocus GetPayload() const noexcept { return {}; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowLostFocus)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Application)
    };

    // -----------------------------------------------------------------------------
    // WindowMoved
    struct WindowMoved
    {
        Int32 X = 0;
        Int32 Y = 0;
    };

    class OPAAX_API WindowMovedEvent final : public Event
    {
    public:
        explicit WindowMovedEvent(const WindowMoved& InData) noexcept : m_Data(InData) {}
        WindowMovedEvent(Int32 InX, Int32 InY) noexcept : m_Data{InX, InY} {}

        FORCEINLINE const WindowMoved& GetPayload() const noexcept { return m_Data; }
        FORCEINLINE Int32 GetX() const noexcept { return m_Data.X; }
        FORCEINLINE Int32 GetY() const noexcept { return m_Data.Y; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowMoved)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory::Application)

    private:
        WindowMoved m_Data;
    };
}
