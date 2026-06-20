#include "CameraControllerSystem.h"

#include "Renderer/RenderSubsystem.h"
#include "ICamera.h"
#include "ICameraController.h"
#include "ShakeCameraController.h"

#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    bool CameraControllerSystem::Startup()
    {
        OPAAX_CORE_INFO("CameraControllerSystem::Startup()");
        return true;
    }

    void CameraControllerSystem::Shutdown()
    {
        OPAAX_CORE_INFO("CameraControllerSystem::Shutdown()");
        DetachAll();
    }

    void CameraControllerSystem::Update(double DeltaTime)
    {
        ICamera& lActive = GetEngineApp()->GetSubsystem<RenderSubsystem>()->GetActiveCamera();

        // Always clear transient offsets — even with no controllers attached the camera
        // should not carry stale shake from a previous PIE session into the next frame.
        lActive.ResetTransientOffsets();

        if (m_Controllers.empty())
        {
            return;
        }

        for (auto& lController : m_Controllers)
        {
            lController->Tick(lActive, DeltaTime);
        }

        // Tail-prune: remove any one-shot controllers that completed this frame.
        for (auto lIt = m_Controllers.begin(); lIt != m_Controllers.end(); )
        {
            if (*lIt && (*lIt)->IsFinished())
            {
                (*lIt)->OnDetach(lActive);
                OPAAX_CORE_INFO("CameraControllerSystem - controller pruned (finished).");
                lIt = m_Controllers.erase(lIt);
            }
            else
            {
                ++lIt;
            }
        }
    }

    void CameraControllerSystem::TriggerShake(const ShakeParams& InParams)
    {
        AttachController(MakeUnique<ShakeCameraController>(InParams));
    }

    void CameraControllerSystem::AttachController(UniquePtr<ICameraController> InController)
    {
        if (!InController)
        {
            return;
        }

        ICamera& lActive = GetEngineApp()->GetSubsystem<RenderSubsystem>()->GetActiveCamera();
        InController->OnAttach(lActive);
        OPAAX_CORE_INFO("CameraControllerSystem - controller attached.");
        m_Controllers.push_back(std::move(InController));
    }

    void CameraControllerSystem::DetachAll()
    {
        if (m_Controllers.empty())
        {
            return;
        }

        ICamera& lActive = GetEngineApp()->GetSubsystem<RenderSubsystem>()->GetActiveCamera();
        for (auto& lController : m_Controllers)
        {
            lController->OnDetach(lActive);
        }
        m_Controllers.clear();
        OPAAX_CORE_INFO("CameraControllerSystem - controllers detached.");
    }

} // namespace Opaax
