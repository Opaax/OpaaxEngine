#include "CameraManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"

#include "Renderer/CameraView.h"

#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/CameraComponent.h"
#include "World/Components/TransformComponent.h"   // WHERE the camera looks from
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"

namespace Opaax
{
    bool CameraManager::Startup()
    {
        m_WorldManager = &OpaaxApplication::GetAppService<IEngine>().GetWorldManager();

        OPAAX_LOG(LogCameraManager, Info, "CameraManager started");
        return true;
    }

    void CameraManager::Shutdown()
    {
        m_WorldManager = nullptr;
        OPAAX_LOG(LogCameraManager, Info, "CameraManager shutdown");
    }

    // =========================================================================
    // Resolve — the rule, with nothing around it. First camera wins; none answers the
    // default frame, so no caller has to special-case an empty world.
    // =========================================================================
    CameraResolution CameraManager::Resolve(World& InWorld)
    {
        CameraResolution lResolution;

        InWorld.Each<TransformComponent, CameraComponent>(
            [&lResolution](EntityID InEntity, TransformComponent& InXf, CameraComponent& InCamera)
            {
                ++lResolution.Count;

                if (lResolution.Count == 1)
                {
                    lResolution.View   = CameraView{ InXf.Position, InCamera.OrthoSize };
                    lResolution.Entity = InEntity;
                }
            });

        return lResolution;
    }

    // =========================================================================
    // Update — Edit worlds are skipped entirely: the editor's camera owns that slot, and
    // this is the ONE place the Edit/Play fork is stated.
    // =========================================================================
    void CameraManager::Update(double /*DeltaTime*/)
    {
        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Play)
        {
            return;
        }

        const CameraResolution lResolution = Resolve(*lWorld);

        // Written UNCONDITIONALLY, so removing the last camera mid-play snaps back to the default
        // frame instead of freezing on the deleted one's last value.
        lWorld->SetCameraView(lResolution.View);

        ReportResolution(*lWorld, lResolution);
    }

    void CameraManager::ReportResolution(World& InWorld, const CameraResolution& InResolution)
    {
        if (InWorld.GetId() == m_ReportedWorld && InResolution.Count == m_ReportedCount)
        {
            return;
        }

        m_ReportedWorld = InWorld.GetId();
        m_ReportedCount = InResolution.Count;

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

        if (InResolution.Count > 1)
        {
            OPAAX_LOG(LogCameraManager, Warn, "World '{}' holds {} CameraComponents — the first is used. "
                                              "Priority/blending is not built.",
                      InWorld.GetName().CStr(), InResolution.Count);
        }
    }
}
