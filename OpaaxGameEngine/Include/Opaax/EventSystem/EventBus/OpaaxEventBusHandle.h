#pragma once
#include "Opaax/OpaaxCoreMacros.h"
#include "Opaax/OpaaxTypes.h"

namespace OPAAX
{
    /**
     * @struct OpaaxEventBusHandle
     * @brief Represents a handle for managing event handlers in the Opaax event bus system.
     *
     * The OpaaxEventBusHandle is used to uniquely identify and manage a specific event handler
     * within the event bus. It encapsulates information such as the event handler function,
     * the type of the event it is associated with, and the unique identifier for the handler.
     */
    struct OPAAX_API OpaaxEventBusHandle
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        OPEventBusFunc m_eventHandlerFunc{};
        OPSTDTypeID m_typeIndex{typeid(void)};
        EventBusHandlerID m_id{};

        //-----------------------------------------------------------------
        // CTOR
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        OpaaxEventBusHandle() = default;
        explicit OpaaxEventBusHandle(OPEventBusFunc InFunc, OPSTDTypeID InTypeID ) : m_eventHandlerFunc(std::move(InFunc)), m_typeIndex{InTypeID}
        {
            static EventBusHandlerID s_nextID{0};
            m_id = ++s_nextID;
        }

        //-----------------------------------------------------------------
        // Functions
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
        /*---------------------------- Getter ----------------------------*/
        FORCEINLINE const   OPEventBusFunc&     GetEventHandlerFunc()   const { return m_eventHandlerFunc; }
        FORCEINLINE         OPSTDTypeID         GetEventType()          const { return m_typeIndex; }
        FORCEINLINE         EventBusHandlerID   GetID()                 const { return m_id; }
    };
}
