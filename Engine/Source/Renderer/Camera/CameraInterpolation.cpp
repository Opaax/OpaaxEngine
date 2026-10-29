#include "Renderer/Camera/CameraInterpolation.h"

#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "World/Components/TransformInterpolationComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/TransformInterpolation.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        // What the last step and the time since did to InCamera's view: its size, and each entity of
        // its chain drawn between steps (TransformInterpolation::BeginFixedStep has just looked at them).
        CameraStepMotion ObserveStep(Entity InCamera, const float InSize, const CameraInterpolationComponent& InHistory)
        {
            CameraStepMotion lMotion;
            if (InHistory.bHasStep)
            {
                lMotion.bInStep    = InHistory.SteppedOrthoSize != InHistory.PreviousOrthoSize;
                lMotion.bSinceStep = InSize != InHistory.SteppedOrthoSize;
                if (lMotion.bInStep && lMotion.bSinceStep)
                {
                    lMotion.MovedBoth = InCamera.GetHandle();
                    lMotion.bSizeBoth = true;
                }
            }

            Uint32 lDepth = 0;
            for (Entity lLink = InCamera; lLink.IsValid() && lDepth < EntityHierarchy::MAX_DEPTH;
                 lLink = EntityHierarchy::GetParent(lLink), ++lDepth)
            {
                const TransformInterpolationComponent* lPose = lLink.TryGet<TransformInterpolationComponent>();
                if (lPose == nullptr) { continue; }

                lMotion.bInStep    = lMotion.bInStep || lPose->bMovedInLastStep;
                lMotion.bSinceStep = lMotion.bSinceStep || lPose->bMovedSinceStep;
                if (lPose->bMovedInLastStep && lPose->bMovedSinceStep && lMotion.MovedBoth == ENTITY_NONE)
                {
                    lMotion.MovedBoth = lLink.GetHandle();
                }
            }
            return lMotion;
        }

        const char* NameOf(Entity InEntity)
        {
            const EntityMeta* lMeta = InEntity.IsValid() ? InEntity.TryGet<EntityMeta>() : nullptr;
            return lMeta != nullptr ? lMeta->Name.CStr() : "<unnamed>";
        }

        void WarnMixed(World& InWorld, Entity InCamera, const CameraRhythm& InRhythm)
        {
            if (InRhythm.bSizeBoth)
            {
                OPAAX_LOG(LogCameraInterpolation, Warn,
                          "Camera '{}' in '{}' jitters on screens faster than the fixed steps: its OrthoSize is changed "
                          "both in fixed steps and between them (in Update), so it is framed between its steps on some "
                          "frames and as it was set on others. Change it in one place: in OnFixedUpdate (or a "
                          "subsystem's FixedUpdate), or in Update only.",
                          NameOf(InCamera), InWorld.GetName().CStr());
                return;
            }

            OPAAX_LOG(LogCameraInterpolation, Warn,
                      "Camera '{}' in '{}' jitters on screens faster than the fixed steps: '{}' is moved both in fixed "
                      "steps and between them (in Update), so it is drawn between its steps on some frames and where it "
                      "was put on others. Move it in one place: in OnFixedUpdate (or a subsystem's FixedUpdate) to "
                      "follow what the steps move, or in Update only. Or split it: a parent moved in the steps, and "
                      "the camera under it offset in Update (a shake).",
                      NameOf(InCamera), InWorld.GetName().CStr(), NameOf(Entity(InRhythm.MovedBoth, &InWorld)));
        }
    }

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
            [&InWorld](const EntityID InId, const CameraComponent& InCamera, CameraInterpolationComponent& InHistory)
            {
                const Entity lCamera(InId, &InWorld);
                if (ObserveCameraRhythm(InHistory.Rhythm, ObserveStep(lCamera, InCamera.OrthoSize, InHistory)))
                {
                    WarnMixed(InWorld, lCamera, InHistory.Rhythm);
                }

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
