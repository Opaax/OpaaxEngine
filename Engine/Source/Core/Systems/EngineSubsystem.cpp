#include "EngineSubsystem.h"

#include "Core/Event/OpaaxEvent.hpp"

void Opaax::EngineSubsystemMgr::DispatchEventAll(OpaaxEvent& Event)
{
    const Uint32 lEventCategories = Event.GetCategoryFlags();
 
    for (auto& lSystem : GetSystems())
    {
        if (lSystem->GetEventCategoryFilter() & lEventCategories)
        {
            lSystem->OnEvent(Event);
        }
    }
}
