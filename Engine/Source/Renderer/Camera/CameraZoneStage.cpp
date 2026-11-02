#include "Renderer/Camera/CameraZoneStage.h"

#include "Renderer/Camera/CameraZoneComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/World.h"

namespace Opaax
{
    void CameraZoneStage::Step(CameraRigPose& InOutPose, const float InDeltaTime)
    {
        World& lWorld = GetWorld();

        m_Zones.clear();
        lWorld.Each<TransformComponent, CameraZoneComponent>(
            [&](const EntityID InEntity, TransformComponent&, CameraZoneComponent& InZone)
            {
                const TransformComponent lPlace = EntityHierarchy::WorldTransform(Entity(InEntity, &lWorld));
                m_Zones.push_back(CameraZone{ InEntity, CameraZoneBounds(InZone, lPlace), InZone.OrthoSize,
                                              InZone.bConfine, InZone.Priority, InZone.BlendTime });
            });

        // The target's zone, or the camera's; a target named and not found keeps the zone applied.
        Vector2F lPoint = InOutPose.Position;
        if (!Target.IsEmpty())
        {
            if (!m_Target.IsValid())
            {
                m_Target = FindEntity(Target);
            }
            if (!m_Target.IsValid())
            {
                const CameraZone* lKept = m_State.Zone.has_value() ? &*m_State.Zone : nullptr;
                StepCameraZone(InOutPose, lKept, m_State, lWorld.GetViewAspect(), InDeltaTime);
                return;
            }
            lPoint = EntityHierarchy::WorldTransform(m_Target).Position;
        }

        StepCameraZone(InOutPose, PickCameraZone(m_Zones, lPoint), m_State, lWorld.GetViewAspect(), InDeltaTime);
    }
}
