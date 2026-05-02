#include "EngineSubsystem.h"

#include "Core/Event/OpaaxEvent.hpp"

void Opaax::EngineSubsystemMgr::UpdateAll(double DeltaTime, bool bAllowPlayOnly)
{
    for (auto& lSystem : GetSystems())
    {
        if (lSystem->IsPlayOnly() && !bAllowPlayOnly) { continue; }
        lSystem->Update(DeltaTime);
    }
}

void Opaax::EngineSubsystemMgr::FixedUpdateAll(double FixedDeltaTime, bool bAllowPlayOnly)
{
    for (auto& lSystem : GetSystems())
    {
        if (lSystem->IsPlayOnly() && !bAllowPlayOnly) { continue; }
        lSystem->FixedUpdate(FixedDeltaTime);
    }
}

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
