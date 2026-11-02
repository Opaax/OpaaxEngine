#include "Renderer/Camera/CameraRigSubsystem.h"

#include "Core/Algo/Algo.h"
#include "Core/Maths/Maths.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraStage.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/World.h"

namespace Opaax
{
    bool CameraRigSubsystem::ShouldCreate(const World& InWorld) const
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    void CameraRigSubsystem::FixedUpdate(const double InFixedDeltaTime)
    {
        World&                    lWorld      = GetWorld();
        const BehaviourSubsystem* lBehaviours = lWorld.GetSubsystems().GetSubsystem<BehaviourSubsystem>();
        if (lBehaviours == nullptr)
        {
            return;
        }

        m_Stages.clear();
        lBehaviours->ForEachStarted(CameraStage::StaticClass(), [this](const EntityID InEntity, Behaviour& InStage)
        {
            m_Stages.push_back(RiggedStage{ InEntity, static_cast<CameraStage*>(&InStage) });
        });

        // Camera by camera, each one's stages by stage; among equals in the order found.
        Algo::StableSort(m_Stages, [](const RiggedStage& InA, const RiggedStage& InB)
        {
            if (InA.Camera != InB.Camera)
            {
                return InA.Camera < InB.Camera;
            }
            return InA.Stage->GetStage() < InB.Stage->GetStage();
        });

        const float lDelta = static_cast<float>(InFixedDeltaTime);
        for (Uint64 lFirst = 0; lFirst < m_Stages.size();)
        {
            const EntityID lId   = m_Stages[lFirst].Camera;
            Uint64         lLast = lFirst + 1;
            while (lLast < m_Stages.size() && m_Stages[lLast].Camera == lId)
            {
                ++lLast;
            }

            Entity           lCamera(lId, &lWorld);
            CameraComponent* lSettings = lCamera.TryGet<CameraComponent>();
            if (lSettings == nullptr)
            {
                if (!m_bWarnedNoCamera)
                {
                    m_bWarnedNoCamera = true;
                    OPAAX_LOG(LogCameraRig, Warn, "'{}' has camera stages but no Camera: they do nothing (said once)",
                              lCamera.GetName());
                }
                lFirst = lLast;
                continue;
            }

            TransformComponent lTransform = EntityHierarchy::WorldTransform(lCamera);
            CameraRigPose      lPose{ lTransform.Position, lTransform.Rotation, lSettings->OrthoSize };
            for (Uint64 lIndex = lFirst; lIndex < lLast; ++lIndex)
            {
                m_Stages[lIndex].Stage->Step(lPose, lDelta);
            }

            lTransform.Position = lPose.Position;
            lTransform.Rotation = lPose.Rotation;
            EntityHierarchy::SetWorldTransform(lCamera, lTransform);
            lSettings->OrthoSize = Maths::Clamp(lPose.OrthoSize, CameraComponent::MIN_ORTHO_SIZE,
                                                CameraComponent::MAX_ORTHO_SIZE);
            lFirst = lLast;
        }
        m_Stages.clear();
    }

    TDynArray<CameraStage*> CameraRigSubsystem::GetStages(const EntityID InCamera) const
    {
        TDynArray<CameraStage*>   lStages;
        const BehaviourSubsystem* lBehaviours = GetWorld().GetSubsystems().GetSubsystem<BehaviourSubsystem>();
        if (lBehaviours == nullptr)
        {
            return lStages;
        }

        lBehaviours->ForEachStarted(CameraStage::StaticClass(), [&lStages, InCamera](const EntityID InEntity, Behaviour& InStage)
        {
            if (InEntity == InCamera)
            {
                lStages.push_back(static_cast<CameraStage*>(&InStage));
            }
        });
        Algo::StableSort(lStages, [](const CameraStage* InA, const CameraStage* InB) { return InA->GetStage() < InB->GetStage(); });
        return lStages;
    }
}
