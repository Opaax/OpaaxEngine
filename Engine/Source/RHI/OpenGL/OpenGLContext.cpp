#include "OpenGLContext.h"

#include "Core/Log/OpaaxLog.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Opaax
{
    // =============================================================================
    // IGraphicsContext factory + window hints
    // =============================================================================
    UniquePtr<IGraphicsContext> IGraphicsContext::Create(EBackend InBackend, void* InNativeWindow)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
                return MakeUnique<OpenGLContext>(static_cast<GLFWwindow*>(InNativeWindow));
        }

        OPAAX_CORE_ERROR("IGraphicsContext::Create — unknown backend; no context created.");
        return nullptr;
    }

    void IGraphicsContext::ApplyWindowHints(EBackend InBackend)
    {
        switch (InBackend)
        {
            // OpenGL: leave GLFW at its defaults (a GL context). No hints — this preserves
            // today's behavior. A Vulkan backend would set GLFW_CLIENT_API = GLFW_NO_API here.
            case EBackend::OpenGL: break;
        }
    }

    // =============================================================================
    // OpenGLContext
    // =============================================================================
    OpenGLContext::OpenGLContext(GLFWwindow* InWindow)
        : m_Window(InWindow)
    {
    }

    bool OpenGLContext::Init()
    {
        OPAAX_CORE_ASSERT(m_Window)

        glfwMakeContextCurrent(m_Window);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            OPAAX_CORE_ERROR("OpenGLContext: glad failed to load OpenGL functions.");
            return false;
        }

        // VSync on by default (matches prior WindowsWindow behavior).
        SetVSync(true);
        return true;
    }

    void OpenGLContext::SwapBuffers()
    {
        glfwSwapBuffers(m_Window);
    }

    void OpenGLContext::SetVSync(bool InEnabled)
    {
        glfwSwapInterval(InEnabled ? 1 : 0);
    }

} // namespace Opaax
