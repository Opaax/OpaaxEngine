#include "GameSubsystem.h"

#include "Core/Event/OpaaxEvent.hpp"

void Opaax::GameSubsystemMgr::UpdateAll(double DeltaTime, bool bAllowPlayOnly)
{
    for (auto& lSystem : GetSystems())
    {
        if (lSystem->IsPlayOnly() && !bAllowPlayOnly) { continue; }
        lSystem->Update(DeltaTime);
    }
}

void Opaax::GameSubsystemMgr::FixedUpdateAll(double FixedDeltaTime, bool bAllowPlayOnly)
{
    for (auto& lSystem : GetSystems())
    {
        if (lSystem->IsPlayOnly() && !bAllowPlayOnly) { continue; }
        lSystem->FixedUpdate(FixedDeltaTime);
    }
}

void Opaax::GameSubsystemMgr::DispatchEventAll(OpaaxEvent& Event)
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
