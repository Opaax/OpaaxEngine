#include "RenderSubsystem.h"

#include "Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/RenderAPI.h"
#include "Core/Config/EngineConfig.h"
#include "Core/Log/OpaaxLog.h"

// glad/GLFW are the OpenGL context bootstrap. They live here (the one sanctioned
// platform/GL touch-point) and are only invoked when the selected backend is OpenGL —
// a future backend brings its own context-init path. WindowsWindow::Init() has already
// called glfwMakeContextCurrent before this runs.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/CoreEngineApp.h"
#include "Core/Window.h"
#include "Core/ApplicationEvents.hpp"
#include "Core/Event/OpaaxEventDispatcher.hpp"

namespace Opaax
{
    bool RenderSubsystem::Startup()
    {
        OPAAX_CORE_INFO("RenderSubsystem::Startup()");

        // Backend comes from engine config (string -> EBackend), not hardcoded.
        const EBackend lBackend = RenderAPI::BackendFromString(EngineConfig::RenderBackend());
        OPAAX_CORE_INFO("RenderSubsystem: render backend = {}", RenderAPI::BackendToString(lBackend));

        // OpenGL-only context bootstrap — load GL function pointers via glad.
        // Other backends never touch glad. glfwMakeContextCurrent must run first
        // (done in WindowsWindow::Init).
        if (lBackend == EBackend::OpenGL)
        {
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                OPAAX_CORE_ERROR("RenderSubsystem: glad failed to load OpenGL functions.");
                return false;
            }
        }

        // RenderCommand takes ownership of the raw ptr (documented on RenderCommand);
        // .release() hands the UniquePtr's payload over to that ownership contract.
        UniquePtr<IRenderAPI> lAPI = RenderAPI::Create(lBackend);
        if (!lAPI)
        {
            OPAAX_CORE_ERROR("RenderSubsystem: backend '{}' produced no IRenderAPI.",
                EngineConfig::RenderBackend());
            return false;
        }
        RenderCommand::Init(lAPI.release());

        Renderer2D::Init();

        // Set initial viewport
        if (GetEngineApp())
        {
            const auto& lWindow = GetEngineApp()->GetWindow();
            RenderCommand::SetViewport(0, 0, lWindow.GetWidth(), lWindow.GetHeight());
        }

        return true;
    }
 
    void RenderSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("RenderSubsystem::Shutdown()");
        Renderer2D::Shutdown();
        RenderCommand::Shutdown();
    }
 
    bool RenderSubsystem::OnEvent(OpaaxEvent& Event)
    {
        OpaaxEventDispatcher lDispatcher(Event);
        lDispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& E) { return OnWindowResize(E); });
        return false;
    }
 
    bool RenderSubsystem::OnWindowResize(WindowResizeEvent& Event)
    {
        RenderCommand::SetViewport(0, 0, Event.GetWidth(), Event.GetHeight());
        return false;
    }
 
} // namespace Opaax