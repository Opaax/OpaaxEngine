#include "CameraControllerSystem.h"

#include "CameraSubsystem.h"
#include "ICamera.h"
#include "ICameraController.h"

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
        if (m_Controllers.empty())
        {
            return;
        }

        ICamera& lActive = GetEngineApp()->GetSubsystem<CameraSubsystem>()->GetActiveCamera();
        for (auto& lController : m_Controllers)
        {
            lController->Tick(lActive, DeltaTime);
        }
    }

    void CameraControllerSystem::AttachController(UniquePtr<ICameraController> InController)
    {
        if (!InController)
        {
            return;
        }

        ICamera& lActive = GetEngineApp()->GetSubsystem<CameraSubsystem>()->GetActiveCamera();
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

        ICamera& lActive = GetEngineApp()->GetSubsystem<CameraSubsystem>()->GetActiveCamera();
        for (auto& lController : m_Controllers)
        {
            lController->OnDetach(lActive);
        }
        m_Controllers.clear();
        OPAAX_CORE_INFO("CameraControllerSystem - controllers detached.");
    }

} // namespace Opaax
