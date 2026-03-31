#include "RenderSubsystem.h"

#include "Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/OpenGL/OpenGLRenderAPI.h"
#include "Core/Log/OpaaxLog.h"
 
// glad must be initialised before any GL calls.
// WindowsWindow::Init() calls glfwMakeContextCurrent which sets up the context,
// but glad itself is loaded here — after the context exists.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/CoreEngineApp.h"
#include "Core/Window.h"
#include "Core/Event/OpaaxEventDispatcher.hpp"

namespace Opaax
{
    bool RenderSubsystem::Startup()
    {
        OPAAX_CORE_INFO("RenderSubsystem::Startup()");
 
        // Load OpenGL function pointers via glad.
        // glfwMakeContextCurrent must have been called first (done in WindowsWindow::Init).
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            OPAAX_CORE_ERROR("RenderSubsystem: glad failed to load OpenGL functions.");
            return false;
        }
 
        // NOTE: new OpenGLRenderAPI() — RenderCommand takes ownership via raw ptr.
        //   This is documented on RenderCommand. Acceptable for a singleton API object.
        RenderCommand::Init(new OpenGLRenderAPI());
 
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