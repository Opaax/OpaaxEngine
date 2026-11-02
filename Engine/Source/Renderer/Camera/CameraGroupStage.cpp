#include "Renderer/Camera/CameraGroupStage.h"

#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/World.h"

namespace Opaax
{
    void CameraGroupStage::Step(CameraRigPose& InOutPose, const float InDeltaTime)
    {
        m_Found.resize(Targets.size());
        m_TargetBounds.reset();

        for (Uint64 lIndex = 0; lIndex < Targets.size(); ++lIndex)
        {
            // Found again when gone or when the name changed.
            Entity& lTarget = m_Found[lIndex];
            if (!lTarget.IsValid() || lTarget.GetName() != Targets[lIndex])
            {
                lTarget = Targets[lIndex].IsEmpty() ? Entity() : FindEntity(Targets[lIndex]);
                if (!lTarget.IsValid())
                {
                    continue;
                }
            }

            const Bounds2D lPoint{ EntityHierarchy::WorldTransform(lTarget).Position, { 0.f, 0.f } };
            if (m_TargetBounds.has_value())
            {
                m_TargetBounds->Encapsulate(lPoint);
            }
            else
            {
                m_TargetBounds = lPoint;
            }
        }

        if (!m_TargetBounds.has_value())
        {
            return;
        }

        const CameraGroupFrame lFrame = FrameCameraGroup(*m_TargetBounds, Padding, GetWorld().GetViewAspect(),
                                                         MinSize, MaxSize);
        InOutPose.Position.x = DampCameraAxis(InOutPose.Position.x, lFrame.Center.x, m_Velocity.x, Damping, InDeltaTime);
        InOutPose.Position.y = DampCameraAxis(InOutPose.Position.y, lFrame.Center.y, m_Velocity.y, Damping, InDeltaTime);
        InOutPose.OrthoSize  = DampCameraAxis(InOutPose.OrthoSize, lFrame.OrthoSize, m_SizeVelocity, Damping, InDeltaTime);
    }
}
