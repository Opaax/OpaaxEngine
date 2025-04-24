#pragma once
#include "Opaax/OpaaxCoreMacros.h"
#include "Opaax/OpaaxTypes.h"

namespace OPAAX
{
    struct OpaaxEventBusHandle
    {
    private:
        OPEventBusFunc m_eventHandlerFunc{};
        OPSTDTypeID m_typeIndex{typeid(void)};
        EventBusHandlerID m_id{};
    public:
        OpaaxEventBusHandle() = default;
        explicit OpaaxEventBusHandle(OPEventBusFunc InFunc, OPSTDTypeID InTypeID ) : m_eventHandlerFunc(std::move(InFunc)), m_typeIndex{InTypeID}
        {
            static EventBusHandlerID s_nextID{0};
            m_id = ++s_nextID;
        }

        FORCEINLINE const OPEventBusFunc& GetEventHandlerFunc() const { return m_eventHandlerFunc; }
        FORCEINLINE OPSTDTypeID GetEventType() const { return m_typeIndex; }
        FORCEINLINE EventBusHandlerID GetID() const { return m_id; }
    };
}
