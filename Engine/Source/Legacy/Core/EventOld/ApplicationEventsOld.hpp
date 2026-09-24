#pragma once
#include "OpaaxEvent.hpp"

namespace Opaax
{
    /**
     * @class WindowCloseEventOld
     */
    class OPAAX_API WindowCloseEventOld final : public OpaaxEvent
    {
    public:
        WindowCloseEventOld() noexcept = default;

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::WindowClose)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
    };

    /**
     * @class WindowResizeEventOld
     */
    class OPAAX_API WindowResizeEventOld final : public OpaaxEvent
    {
    public:
        WindowResizeEventOld(Uint32 InWidth, Uint32 InHeight) noexcept
            : m_Width(InWidth), m_Height(InHeight)
        {}

        FORCEINLINE Uint32 GetWidth()  const noexcept { return m_Width; }
        FORCEINLINE Uint32 GetHeight() const noexcept { return m_Height; }

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::WindowResize)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)

    private:
        Uint32 m_Width;
        Uint32 m_Height;
    };

    /**
     * @class WindowFocusEventOld
     */
    class OPAAX_API WindowFocusEventOld final : public OpaaxEvent
    {
    public:
        WindowFocusEventOld() noexcept = default;

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::WindowFocus)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
    };

    /**
     * @class WindowLostFocusEventOld
     */
    class OPAAX_API WindowLostFocusEventOld final : public OpaaxEvent
    {
    public:
        WindowLostFocusEventOld() noexcept = default;

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::WindowLostFocus)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
    };

    /**
     * @class WindowMovedEventOld
     */
    class OPAAX_API WindowMovedEventOld final : public OpaaxEvent
    {
    public:
        WindowMovedEventOld(Int32 InX, Int32 InY) noexcept
            : m_X(InX), m_Y(InY)
        {}

        FORCEINLINE Int32 GetX() const noexcept { return m_X; }
        FORCEINLINE Int32 GetY() const noexcept { return m_Y; }

        OPAAX_EVENT_CLASS_TYPE(EEventTypeOld::WindowMoved)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)

    private:
        Int32 m_X;
        Int32 m_Y;
    };
}
