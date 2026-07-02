#include "EngineSubsystem.h"

#include "Core/Event/OpaaxEvent.hpp"

void Opaax::EngineSubsystemMgrOld::UpdateAll(double DeltaTime, bool bAllowPlayOnly)
{
    for (auto& lSystem : GetSystems())
    {
        if (lSystem->IsPlayOnly() && !bAllowPlayOnly) { continue; }
        lSystem->Update(DeltaTime);
    }
}

void Opaax::EngineSubsystemMgrOld::FixedUpdateAll(double FixedDeltaTime, bool bAllowPlayOnly)
{
    for (auto& lSystem : GetSystems())
    {
        if (lSystem->IsPlayOnly() && !bAllowPlayOnly) { continue; }
        lSystem->FixedUpdate(FixedDeltaTime);
    }
}

void Opaax::EngineSubsystemMgrOld::OnPlayBeginAll()
{
    for (auto& lSystem : GetSystems())
    {
        lSystem->OnPlayBegin();
    }
}

void Opaax::EngineSubsystemMgrOld::OnPlayEndAll()
{
    const auto& lSystems = GetSystems();
    for (auto it = lSystems.rbegin(); it != lSystems.rend(); ++it)
    {
        (*it)->OnPlayEnd();
    }
}

void Opaax::EngineSubsystemMgrOld::DispatchEventAll(OpaaxEvent& Event)
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
