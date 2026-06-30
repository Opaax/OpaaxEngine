#include "OpenGLContext.h"

#include "Core/Log/OpaaxLog.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Application/Services/ILogger.h"

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
        
        OPAAX_LOG(LogOpenGLContext, Info, "====================  Render Backend  ====================")
        OPAAX_LOG(LogOpenGLContext, Info, "  API .............. OpenGL {}", lStr(GL_VERSION))
        OPAAX_LOG(LogOpenGLContext, Info, "  GPU .............. {}",         lStr(GL_RENDERER));
        OPAAX_LOG(LogOpenGLContext, Info, "  Vendor ........... {}",         lStr(GL_VENDOR));
        OPAAX_LOG(LogOpenGLContext, Info, "  GLSL ............. {}",         lStr(GL_SHADING_LANGUAGE_VERSION));
        OPAAX_LOG(LogOpenGLContext, Info, "  Texture units .... {}",         lMaxTexUnits);
        OPAAX_LOG(LogOpenGLContext, Info, "  Max texture size . {}",         lMaxTexSize);
        OPAAX_LOG(LogOpenGLContext, Info, "  VSync ............ on");
        OPAAX_LOG(LogOpenGLContext, Info, "==========================================================");
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
