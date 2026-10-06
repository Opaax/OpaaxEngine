#pragma once
#include "OpaaxEvent.hpp"

namespace Opaax
{
    /**
    * Stack-local helper. Construct from any OpaaxEvent ref, call Dispatch<T> with a callable.
    * If the event type matches T, the callable is invoked and
    * bHandled is set based on its return value.
    *
    * Usage:
    * void OnEvent(OpaaxEvent& Event)
    * {
    *   EventDispatcher lDispatcher(Event);
    *   lDispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& E) { return OnWindowClose(E); });}
    */
    class OPAAX_API OpaaxEventDispatcher
    {
    public:
        explicit OpaaxEventDispatcher(OpaaxEvent& InEvent) noexcept
            : m_Event(InEvent)
        {}
        
        /**
         * 
         * @tparam T 
         * @tparam TFunc must be callable as: bool(T&)
         * @param Func 
         * @return Returns true if the event type matched and the handler was called.
         */
        template<typename T, typename TFunc>
        bool Dispatch(const TFunc& Func)
        {
            static_assert(std::is_base_of_v<OpaaxEvent, T>, "EventDispatcher::Dispatch — T must derive from OpaaxEvent");
 
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.bHandled |= Func(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }
 
    private:
        OpaaxEvent& m_Event;
    };
}
