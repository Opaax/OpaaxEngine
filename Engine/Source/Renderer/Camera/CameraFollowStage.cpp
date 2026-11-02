#include "Renderer/Camera/CameraFollowStage.h"

#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/World.h"

namespace Opaax
{
    void CameraFollowStage::Step(CameraRigPose& InOutPose, const float InDeltaTime)
    {
        if (!m_Target.IsValid())
        {
            if (Target.IsEmpty())
            {
                return;
            }

            // Found again, it may be somewhere else: its velocity starts over.
            m_Target           = FindEntity(Target);
            m_State.bHasTarget = false;
            if (!m_Target.IsValid())
            {
                return;
            }
        }

        const Vector2F lTarget = EntityHierarchy::WorldTransform(m_Target).Position;
        const Vector2F lHalfExtent{ InOutPose.OrthoSize * GetWorld().GetViewAspect(), InOutPose.OrthoSize };
        InOutPose.Position = StepCameraFollow(InOutPose.Position, lTarget, lHalfExtent, GetSettings(), m_State,
                                              InDeltaTime);
    }
}
