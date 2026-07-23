#pragma once

#include "Core/EngineAPI.h"
#include "Core/Events/EventTypes.hpp"

namespace Opaax
{
    // =============================================================================
    // Event — Tier-1 base
    // =============================================================================

    /**
     * @class Event
     * Events are stack-allocated value types
     * never heap-allocated, never stored past the callback that produced them. A
     * receiver stops propagation by returning true from its EventDispatcher handler
     * (the dispatcher ORs that into bHandled). Concrete events stamp their identity
     * with the OPAAX_EVENT_CLASS_TYPE / _CATEGORY macros below.
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
     * @class EventDispatcher
     * Stack-constructed around a live Event reference. Dispatch<T> matches the event's
     * runtime type against T::GetStaticType() (enum compare — no RTTI / dynamic_cast).
     * On match it invokes the handler with the down-cast event and ORs the handler's
     * bool return into the event's bHandled.
     *
     * The handler is taken as a deduced functor (not TFunction) so a lambda binds with
     * zero heap allocation — this runs on the per-keystroke / per-mouse-move path.
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
         * Run InHandler iff the live event is a T. The handler returns bool ("handled").
         * @return true if T matched the event's type (handler ran).
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
    // Stamping macros — implement the Event interface on a concrete event class.
    //
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
