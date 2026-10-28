#include "Renderer/Camera/CameraInterpolation.h"

#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "World/Entity/TransformInterpolation.h"
#include "World/World.h"

namespace Opaax
{
    void CameraInterpolation::RecordSteps(Entity InCamera)
    {
        if (!InCamera.IsValid() || InCamera.Has<CameraInterpolationComponent>()) { return; }

        InCamera.Add<CameraInterpolationComponent>();
    }

    void CameraInterpolation::Cut(Entity InCamera)
    {
        const CameraComponent* lCamera = InCamera.IsValid() ? InCamera.TryGet<CameraComponent>() : nullptr;
        if (lCamera == nullptr) { return; }

        RecordSteps(InCamera);
        TransformInterpolation::Teleport(InCamera);

        CameraInterpolationComponent& lSizes = InCamera.Get<CameraInterpolationComponent>();
        lSizes.PreviousOrthoSize = lCamera->OrthoSize;
        lSizes.SteppedOrthoSize  = lCamera->OrthoSize;
        lSizes.bHasStep          = true;
        lSizes.bCut              = true;
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
