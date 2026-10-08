#include "Renderer/Camera/CameraManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"

#include "Renderer/CameraView.h"

#include "World/World.h"
#include "World/WorldManager.h"
#include "Renderer/Camera/CameraComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"

namespace Opaax
{
    bool CameraManager::Startup()
    {
        m_WorldManager = &OpaaxApplication::GetAppService<IEngine>().GetWorldManager();

        return true;
    }

    void CameraManager::Shutdown()
    {
        m_WorldManager = nullptr;
    }

    // =========================================================================
    // Resolve — the highest priority wins, the first found among equals; no camera gives the
    //   default view.
    // =========================================================================
    CameraResolution CameraManager::Resolve(World& InWorld)
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
                    const Vector2F lFrom = EntityHierarchy::WorldTransform(Entity{ InEntity, &InWorld }).Position;

                    lResolution.View   = CameraView{ lFrom, InCamera.OrthoSize };
                    lResolution.Entity = InEntity;
                    lResolution.Tied   = 1;
                    lBestPriority      = InCamera.Priority;
                }
            });

        return lResolution;
    }

    // =========================================================================
    // Update — Edit worlds are skipped: the editor's camera sets their view.
    // =========================================================================
    void CameraManager::Update(double /*DeltaTime*/)
    {
        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Play)
        {
            return;
        }

        const CameraResolution lResolution = Resolve(*lWorld);

        // Always written, so removing the last camera resets to the default view.
        lWorld->SetCameraView(lResolution.View);

        ReportResolution(*lWorld, lResolution);
    }

    void CameraManager::ReportResolution(World& InWorld, const CameraResolution& InResolution)
    {
        if (InWorld.GetId() == m_ReportedWorld && InResolution.Count == m_ReportedCount
            && InResolution.Entity == m_ReportedCamera)
        {
            return;
        }

        m_ReportedWorld  = InWorld.GetId();
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
