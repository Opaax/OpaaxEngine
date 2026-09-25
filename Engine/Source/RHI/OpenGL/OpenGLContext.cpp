#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Log/Logger.h"

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
            OPAAX_LOG(LogOpenGLContext, Error, "Failed to initialize GLAD");
            return false;
        }

        // VSync on by default (matches prior WindowsWindow behavior).
        SetVSync(true);

        LogAdapterInfo();
        return true;
    }

    void OpenGLContext::LogAdapterInfo() const
    {
        auto lStr = [](GLenum InName) -> const char*
        {
            const GLubyte* lValue = glGetString(InName);
            return lValue ? reinterpret_cast<const char*>(lValue) : "<unknown>";
        };

        GLint lMaxTexUnits = 0, lMaxTexSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &lMaxTexUnits);
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,        &lMaxTexSize);
        
        // One line: what a bug report needs to know about the machine.
        OPAAX_LOG(LogOpenGLContext, Info, "OpenGL {} on {} ({}) — GLSL {}, {} texture units, max texture {}, VSync on",
                  lStr(GL_VERSION), lStr(GL_RENDERER), lStr(GL_VENDOR), lStr(GL_SHADING_LANGUAGE_VERSION),
                  lMaxTexUnits, lMaxTexSize);
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
