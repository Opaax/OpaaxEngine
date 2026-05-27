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

        if (!m_OwnedCamera)
        {
            m_OwnedCamera = MakeUnique<OrthographicCamera>();
        }
        m_ActivePtr = m_OwnedCamera.get();

        if (GetEngineApp())
        {
            const auto& lWindow = GetEngineApp()->GetWindow();
            // Seed the cached size from the window so cameras swapped before the first
            // ViewportPanel resize event still get a non-degenerate projection.
            m_LastViewportWidth  = lWindow.GetWidth();
            m_LastViewportHeight = lWindow.GetHeight();
            m_ActivePtr->SetViewportSize(m_LastViewportWidth, m_LastViewportHeight);
        }

        return true;
    }

    void CameraSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("CameraSubsystem::Shutdown()");
        m_ActivePtr = nullptr;
        m_OwnedCamera.reset();
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

        // Owning swap always destroys whatever was previously owned, even if a
        // non-owning camera is currently active — the prior owned camera is
        // structurally inaccessible once the new one moves in.
        m_OwnedCamera = std::move(InCamera);
        m_ActivePtr   = m_OwnedCamera.get();
        if (m_LastViewportWidth > 0 && m_LastViewportHeight > 0)
        {
            m_ActivePtr->SetViewportSize(m_LastViewportWidth, m_LastViewportHeight);
        }
        OPAAX_CORE_INFO("CameraSubsystem - active camera swapped (owning).");
    }

    void CameraSubsystem::SetActiveCameraNonOwning(ICamera* InCamera)
    {
        m_ActivePtr = InCamera;
        if (InCamera)
        {
            if (m_LastViewportWidth > 0 && m_LastViewportHeight > 0)
            {
                InCamera->SetViewportSize(m_LastViewportWidth, m_LastViewportHeight);
            }
            OPAAX_CORE_INFO("CameraSubsystem - active camera swapped (non-owning).");
        }
        else
        {
            OPAAX_CORE_INFO("CameraSubsystem - active camera cleared.");
        }
    }

    ICamera& CameraSubsystem::GetActiveCamera()
    {
        return *m_ActivePtr;
    }

    void CameraSubsystem::SetViewportSize(Uint32 InWidth, Uint32 InHeight)
    {
        m_LastViewportWidth  = InWidth;
        m_LastViewportHeight = InHeight;
        if (m_ActivePtr)
        {
            m_ActivePtr->SetViewportSize(InWidth, InHeight);
        }
    }

    bool CameraSubsystem::OnWindowResize(WindowResizeEvent& Event)
    {
        SetViewportSize(Event.GetWidth(), Event.GetHeight());
        return false;
    }

} // namespace Opaax
