#include "Renderer/Camera/CameraManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Engine/Config/Config_Engine.h"

#include "Renderer/CameraView.h"

#include "World/World.h"
#include "World/WorldManager.h"
#include "Renderer/Camera/CameraComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/TransformInterpolation.h"

namespace Opaax
{
    bool CameraManager::Startup()
    {
        m_WorldManager = &OpaaxApplication::GetAppService<IEngine>().GetWorldManager();
        m_EngineConfig = &OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>();

        return true;
    }

    void CameraManager::Shutdown()
    {
        m_WorldManager = nullptr;
        m_EngineConfig = nullptr;
    }

    // =========================================================================
    // Resolve — the highest priority wins, the first found among equals; no camera gives the
    //   default view.
    // =========================================================================
    CameraResolution CameraManager::Resolve(World& InWorld, const TOptional<float> InAlpha)
    {
        CameraResolution lResolution;
        Int32            lBestPriority = 0;

        InWorld.Each<TransformComponent, CameraComponent>(
            [&](EntityID InEntity, TransformComponent&, CameraComponent& InCamera)
            {
                ++lResolution.Count;

                if (lResolution.Count > 1 && InCamera.Priority == lBestPriority)
                {
                    ++lResolution.Tied;
                    return;
                }

                if (lResolution.Count == 1 || InCamera.Priority > lBestPriority)
                {
                    // World transform: a camera parented to the player follows it.
                    const Entity   lCamera{ InEntity, &InWorld };
                    const Vector2F lFrom = InAlpha.has_value() ? EntityHierarchy::DisplayTransform(lCamera, *InAlpha).Position
                                                               : EntityHierarchy::WorldTransform(lCamera).Position;

                    lResolution.View   = CameraView{ lFrom, InCamera.OrthoSize };
                    lResolution.Entity = InEntity;
                    lResolution.Tied   = 1;
                    lBestPriority      = InCamera.Priority;
                }
            });

        return lResolution;
    }

    void CameraManager::RecordCameraSteps(World& InWorld)
    {
        InWorld.Each<CameraComponent>([&InWorld](EntityID InEntity, CameraComponent&)
        {
            TransformInterpolation::RecordSteps(Entity{ InEntity, &InWorld });
        });
    }

    // =========================================================================
    // Update — after the world's update (this subsystem is added after WorldManager), before the
    //   fixed steps: a camera added this frame is recorded from its first step.
    // =========================================================================
    void CameraManager::Update(double /*InDeltaTime*/)
    {
        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        if (lWorld != nullptr && lWorld->GetMode() == EWorldMode::Play)
        {
            RecordCameraSteps(*lWorld);
        }
    }

    // =========================================================================
    // Render — before the renderer draws (it is added after this subsystem), after the fixed steps.
    //   Edit worlds are skipped: the editor's camera sets their view.
    // =========================================================================
    void CameraManager::Render(const double InAlpha)
    {
        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Play)
        {
            return;
        }

        // Interpolation off: the camera is where it is, like the bodies.
        const bool lInterpolate = m_EngineConfig == nullptr || m_EngineConfig->GetData().Render.bInterpolation;

        const CameraResolution lResolution = lInterpolate ? Resolve(*lWorld, static_cast<float>(InAlpha))
                                                          : Resolve(*lWorld);

        // Always written, so removing the last camera resets to the default view.
        lWorld->SetCameraView(lResolution.View);

        ReportResolution(*lWorld, lResolution);
    }

    void CameraManager::ReportResolution(World& InWorld, const CameraResolution& InResolution)
    {
        if (InWorld.GetHandle() == m_ReportedWorld && InResolution.Count == m_ReportedCount
            && InResolution.Entity == m_ReportedCamera)
        {
            return;
        }

        m_ReportedWorld  = InWorld.GetHandle();
        m_ReportedCount  = InResolution.Count;
        m_ReportedCamera = InResolution.Entity;

        if (InResolution.Count == 0)
        {
            OPAAX_LOG(LogCameraManager, Warn, "Play world '{}' has no CameraComponent — framing with the default centred view",
                      InWorld.GetName().CStr());
            return;
        }

        Entity            lEntity = Entity(InResolution.Entity, &InWorld);
        const EntityMeta* lMeta   = lEntity.TryGet<EntityMeta>();

        OPAAX_LOG(LogCameraManager, Info, "Active camera in '{}': entity '{}' at ({}, {}), orthoSize {}",
                  InWorld.GetName().CStr(),
                  lMeta != nullptr ? lMeta->Name.CStr() : "<unnamed>",
                  InResolution.View.Position.x, InResolution.View.Position.y, InResolution.View.OrthoSize);

        if (InResolution.Tied > 1)
        {
            OPAAX_LOG(LogCameraManager, Warn, "World '{}': {} cameras share the highest Priority — the first found is "
                                              "used; raise the one that should frame the world",
                      InWorld.GetName().CStr(), InResolution.Tied);
        }
    }
}
