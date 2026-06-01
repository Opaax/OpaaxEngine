#include "OpenGLContext.h"

#include "Core/Log/OpaaxLog.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Opaax
{
    // NOTE: IGraphicsContext::Create + ApplyWindowHints (backend dispatch) live in
    //   RHI/BackendFactory.cpp. This file holds only the OpenGL context impl.

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
