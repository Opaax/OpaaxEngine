#pragma once
#include "Event/OpaaxEvent.hpp"

namespace Opaax
{
    /**
     * @class WindowCloseEvent
     */
    class OPAAX_API WindowCloseEvent final : public OpaaxEvent
    {
    public:
        WindowCloseEvent() noexcept = default;
 
        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowClose)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
    };

    /**
     * @class WindowResizeEvent
     */
    class OPAAX_API WindowResizeEvent final : public OpaaxEvent
    {
    public:
        WindowResizeEvent(Uint32 InWidth, Uint32 InHeight) noexcept
            : m_Width(InWidth), m_Height(InHeight)
        {}
 
        FORCEINLINE Uint32 GetWidth()  const noexcept { return m_Width; }
        FORCEINLINE Uint32 GetHeight() const noexcept { return m_Height; }
 
        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowResize)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
 
    private:
        Uint32 m_Width;
        Uint32 m_Height;
    };

    /**
     * @class WindowFocusEvent
     */
    class OPAAX_API WindowFocusEvent final : public OpaaxEvent
    {
    public:
        WindowFocusEvent() noexcept = default;
 
        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowFocus)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
    };

    /**
     * @class WindowLostFocusEvent
     */
    class OPAAX_API WindowLostFocusEvent final : public OpaaxEvent
    {
    public:
        WindowLostFocusEvent() noexcept = default;
 
        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowLostFocus)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
    };

    /**
     * @class WindowMovedEvent
     */
    class OPAAX_API WindowMovedEvent final : public OpaaxEvent
    {
    public:
        WindowMovedEvent(Int32 InX, Int32 InY) noexcept
            : m_X(InX), m_Y(InY)
        {}
 
        FORCEINLINE Int32 GetX() const noexcept { return m_X; }
        FORCEINLINE Int32 GetY() const noexcept { return m_Y; }
 
        OPAAX_EVENT_CLASS_TYPE(EEventType::WindowMoved)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
 
    private:
        Int32 m_X;
        Int32 m_Y;
    };
}
