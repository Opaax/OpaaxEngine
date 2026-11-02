#include "Renderer/Camera/CameraSpeedZoomStage.h"

#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"

namespace Opaax
{
    void CameraSpeedZoomStage::Step(CameraRigPose& InOutPose, const float InDeltaTime)
    {
        // The size it starts from, and the base when BaseSize is 0.
        if (!m_bStarted)
        {
            m_bStarted   = true;
            m_StartSize  = InOutPose.OrthoSize;
            m_State.Size = InOutPose.OrthoSize;
        }

        if (!m_Target.IsValid())
        {
            if (Target.IsEmpty())
            {
                return;
            }

            // Found again, it may be somewhere else: its speed starts over.
            m_Target           = FindEntity(Target);
            m_State.bHasTarget = false;
            if (!m_Target.IsValid())
            {
                return;
            }
        }

        const CameraSpeedZoomSettings lSettings{ BaseSize > 0.f ? BaseSize : m_StartSize, ZoomOut, FullSpeed, HalfLife };
        InOutPose.OrthoSize = StepCameraSpeedZoom(EntityHierarchy::WorldTransform(m_Target).Position, lSettings, m_State,
                                                  InDeltaTime);
    }
}
