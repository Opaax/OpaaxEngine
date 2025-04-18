#include "Opaax/EventSystem/EventBus/OpaaxEventBus.h"
#include <ranges>


OPAAX::OpaaxEventBus::~OpaaxEventBus()
{
    UnBindAll();
}

bool OPAAX::OpaaxEventBus::UnBind(const OpaaxEventBusHandle& Handle)
{
    std::unique_lock<std::mutex> lLock(m_mutex);
            
    auto& lSubscribers = m_listeners[Handle.GetEventType()];

    auto lIt = std::remove_if(lSubscribers.begin(), lSubscribers.end(),
                             [Handle](const OpaaxEventBusHandle& InSubscribers)
                             {
                                 return InSubscribers.GetID() == Handle.GetID();
                             });

    bool lRemoved = lIt != lSubscribers.end();
    lSubscribers.erase(lIt);
            
    return lRemoved;
}

void OPAAX::OpaaxEventBus::UnBindAll()
{
    for (auto lHandlesVals : m_listeners | std::views::values)
    {
        lHandlesVals.clear();
    }

    m_listeners.clear();
}