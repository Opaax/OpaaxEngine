#include "PhysicsEventSystem.h"

#include "Core/CoreEngineApp.h"
#include "Core/Event/OpaaxEventDispatcher.hpp"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxStringID.hpp"

#include "Physics/Events/PhysicsEvents.h"
#include "World/World.h"
#include "ECS/Components/TagComponent.h"

using namespace Opaax;
using namespace Opaax::ECS;

namespace
{
    OpaaxString TagName(World& InWorld, EntityID InEntity)
    {
        if (const TagComponent* lTag = InWorld.GetComponent<TagComponent>(InEntity))
        {
            return lTag->Tag.ToString();
        }
        return OpaaxString("<none>");
    }
}

Uint32 PhysicsEventSystem::GetEventCategoryFilter() const noexcept
{
    return EEventCategory_Physics;
}

bool PhysicsEventSystem::OnEvent(OpaaxEvent& InEvent)
{
    CoreEngineApp* lApp = GetEngineApp();
    if (lApp == nullptr) { return false; }
    World& lWorld = lApp->GetWorld();

    OpaaxEventDispatcher lDispatcher(InEvent);

    lDispatcher.Dispatch<OnCollisionEnterEvent>([&](OnCollisionEnterEvent& E) -> bool
    {
        const OpaaxString lA = TagName(lWorld, E.GetEntityA());
        const OpaaxString lB = TagName(lWorld, E.GetEntityB());
        OPAAX_CORE_INFO("[Sandbox] Collision ENTER: {} <-> {}", lA, lB);
        return false;
    });

    lDispatcher.Dispatch<OnCollisionExitEvent>([&](OnCollisionExitEvent& E) -> bool
    {
        const OpaaxString lA = TagName(lWorld, E.GetEntityA());
        const OpaaxString lB = TagName(lWorld, E.GetEntityB());
        OPAAX_CORE_INFO("[Sandbox] Collision EXIT:  {} <-> {}", lA, lB);
        return false;
    });

    lDispatcher.Dispatch<OnOverlapStartEvent>([&](OnOverlapStartEvent& E) -> bool
    {
        const EntityID lSensor  = E.GetOverlapEntity();
        const EntityID lVisitor = E.GetOtherEntity();
        const OpaaxString lSensorName  = TagName(lWorld, lSensor);
        const OpaaxString lVisitorName = TagName(lWorld, lVisitor);
        OPAAX_CORE_INFO("[Sandbox] Overlap START: sensor '{}' visited by '{}'", lSensorName, lVisitorName);

        // Pickup: destroy the sensor entity in the handler. The engine's ReconcileDeadBodies
        // removes its orphaned sensor body on the next step (no phantom OnOverlapTick survives).
        if (lSensorName == OpaaxString("Pickup") && lWorld.IsValid(lSensor))
        {
            ++m_Score;
            OPAAX_CORE_INFO("[Sandbox] Pickup collected! Score = {}", m_Score);
            lWorld.DestroyEntity(lSensor);
        }
        return false;
    });

    lDispatcher.Dispatch<OnOverlapStopEvent>([&](OnOverlapStopEvent& E) -> bool
    {
        const OpaaxString lSensorName  = TagName(lWorld, E.GetOverlapEntity());
        const OpaaxString lVisitorName = TagName(lWorld, E.GetOtherEntity());
        OPAAX_CORE_INFO("[Sandbox] Overlap STOP:  sensor '{}' left by '{}'", lSensorName, lVisitorName);
        return false;
    });

    lDispatcher.Dispatch<OnExitWorldBoundsEvent>([&](OnExitWorldBoundsEvent& E) -> bool
    {
        const Vector2F lPos = E.GetLastPosition();
        const OpaaxString lName = TagName(lWorld, E.GetEntity());
        OPAAX_CORE_INFO("[Sandbox] World-bounds KILL: '{}' left bounds at ({}, {})", lName, lPos.x, lPos.y);
        return false;
    });

    return false;
}
