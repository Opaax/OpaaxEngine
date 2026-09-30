#pragma once

#include "Core/EngineAPI.h"
#include "Core/Events/EventTypes.hpp"

namespace Opaax
{
    // =============================================================================
    // Event
    // =============================================================================

    /**
     * Base window/input event. Stack-allocated, never stored.
     * A handler returning true marks it handled.
     */
    class OPAAX_API Event
    {
        friend class EventDispatcher;

        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~Event() = default;

        // =============================================================================
        // Functions
        // =============================================================================

        // -----------------------------------------------------------------------------
        // Getters
    public:
        virtual EEventType  GetEventType()     const noexcept = 0;
        virtual const char* GetName()          const noexcept = 0;
        virtual Uint16      GetCategoryFlags() const noexcept = 0;

        FORCEINLINE bool IsInCategory(EEventCategory InCategory) const noexcept
        {
            return (GetCategoryFlags() & static_cast<Uint16>(InCategory)) != 0;
        }

        FORCEINLINE bool IsHandled() const noexcept { return bHandled; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool bHandled = false;
    };

    // =============================================================================
    // EventDispatcher
    // =============================================================================

    /**
     * Dispatches an event to a handler by type (no RTTI, no allocation).
     */
    class EventDispatcher
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit EventDispatcher(Event& InEvent) noexcept : m_Event(InEvent) {}

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Runs InHandler if the event is a T. The handler returns true if it handled the event.
         * @return True if the event is a T
         */
        template<typename T, typename TFunc>
        bool Dispatch(TFunc&& InHandler)
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.bHandled |= InHandler(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Event& m_Event;
    };

    // =============================================================================
    // Implement the Event interface on a concrete event class:
    //   class WindowResizeEvent : public Event {
    //       OPAAX_EVENT_CLASS_TYPE(EEventType::WindowResize)
    //       OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Application)
    //   };
    // =============================================================================
#define OPAAX_EVENT_CLASS_TYPE(Type) \
    static  ::Opaax::EEventType GetStaticType() noexcept                { return Type; } \
    virtual ::Opaax::EEventType GetEventType()  const noexcept override { return GetStaticType(); } \
    virtual const char*         GetName()       const noexcept override { return #Type; }

#define OPAAX_EVENT_CLASS_CATEGORY(Category) \
    virtual ::Opaax::Uint16 GetCategoryFlags() const noexcept override { return static_cast<::Opaax::Uint16>(Category); }
}
