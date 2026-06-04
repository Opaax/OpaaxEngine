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

        OPAAX_CORE_INFO("====================  Render Backend  ====================");
        OPAAX_CORE_INFO("  API .............. OpenGL {}", lStr(GL_VERSION));
        OPAAX_CORE_INFO("  GPU .............. {}",         lStr(GL_RENDERER));
        OPAAX_CORE_INFO("  Vendor ........... {}",         lStr(GL_VENDOR));
        OPAAX_CORE_INFO("  GLSL ............. {}",         lStr(GL_SHADING_LANGUAGE_VERSION));
        OPAAX_CORE_INFO("  Texture units .... {}",         lMaxTexUnits);
        OPAAX_CORE_INFO("  Max texture size . {}",         lMaxTexSize);
        OPAAX_CORE_INFO("  VSync ............ on");
        OPAAX_CORE_INFO("==========================================================");
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
