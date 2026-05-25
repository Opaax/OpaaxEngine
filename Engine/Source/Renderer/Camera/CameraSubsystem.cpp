#include "CameraSubsystem.h"

#include "OrthographicCamera.h"

#include "Core/CoreEngineApp.h"
#include "Core/Window.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/Event/OpaaxEventDispatcher.hpp"

namespace Opaax
{
    bool CameraSubsystem::Startup()
    {
        OPAAX_CORE_INFO("CameraSubsystem::Startup()");

        if (!m_ActiveCamera)
        {
            m_ActiveCamera = MakeUnique<OrthographicCamera>();
        }

        if (GetEngineApp())
        {
            const auto& lWindow = GetEngineApp()->GetWindow();
            m_ActiveCamera->SetViewportSize(lWindow.GetWidth(), lWindow.GetHeight());
        }

        return true;
    }

    void CameraSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("CameraSubsystem::Shutdown()");
        m_ActiveCamera.reset();
    }

    bool CameraSubsystem::OnEvent(OpaaxEvent& Event)
    {
        OpaaxEventDispatcher lDispatcher(Event);
        lDispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& InEvent) { return OnWindowResize(InEvent); }
        );
        return false;
    }

    void CameraSubsystem::SetActiveCamera(UniquePtr<ICamera> InCamera)
    {
        if (!InCamera)
        {
            OPAAX_CORE_WARN("CameraSubsystem::SetActiveCamera - null camera ignored.");
            return;
        }

        m_ActiveCamera = std::move(InCamera);
        OPAAX_CORE_INFO("CameraSubsystem - active camera swapped.");
    }

    ICamera& CameraSubsystem::GetActiveCamera()
    {
        return *m_ActiveCamera;
    }

    void CameraSubsystem::SetViewportSize(Uint32 InWidth, Uint32 InHeight)
    {
        if (m_ActiveCamera)
        {
            m_ActiveCamera->SetViewportSize(InWidth, InHeight);
        }
    }

    bool CameraSubsystem::OnWindowResize(WindowResizeEvent& Event)
    {
        SetViewportSize(Event.GetWidth(), Event.GetHeight());
        return false;
    }

} // namespace Opaax
