#include "Renderer/Camera/CameraInterpolation.h"

#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "World/World.h"

namespace Opaax
{
    void CameraInterpolation::RecordSteps(Entity InCamera)
    {
        if (!InCamera.IsValid() || InCamera.Has<CameraInterpolationComponent>()) { return; }

        InCamera.Add<CameraInterpolationComponent>();
    }

    void CameraInterpolation::StopRecordingSteps(Entity InCamera)
    {
        if (!InCamera.IsValid()) { return; }

        // Only the cameras recorded carry it: stale sizes would blend toward a zoom from before.
        InCamera.Remove<CameraInterpolationComponent>();
    }

    void CameraInterpolation::BeginFixedStep(World& InWorld)
    {
        InWorld.Each<CameraComponent, CameraInterpolationComponent>(
            [](EntityID, const CameraComponent& InCamera, CameraInterpolationComponent& InHistory)
            {
                InHistory.PreviousOrthoSize = InCamera.OrthoSize;
            });
    }

    void CameraInterpolation::EndFixedStep(World& InWorld)
    {
        InWorld.Each<CameraComponent, CameraInterpolationComponent>(
            [](EntityID, const CameraComponent& InCamera, CameraInterpolationComponent& InHistory)
            {
                InHistory.SteppedOrthoSize = InCamera.OrthoSize;
                InHistory.bHasStep         = true;
            });
    }
}
