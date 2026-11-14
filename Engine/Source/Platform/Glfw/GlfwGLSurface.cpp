#include "Platform/Glfw/GlfwGLSurface.h"

#include <GLFW/glfw3.h>

#include "Core/Log/Logger.h"

namespace Opaax
{
    namespace
    {
        /** glfwGetProcAddress in the loader's shape: GLFW answers a function pointer, the loader a void*. */
        void* LoadGLProc(const char* InName)
        {
            return reinterpret_cast<void*>(glfwGetProcAddress(InName));
        }
    }

    // =============================================================================
    // Statics
    // =============================================================================
    void GlfwGLSurface::ApplyHints(const GLSurfaceDesc& InDesc)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, static_cast<int>(InDesc.MajorVersion));
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, static_cast<int>(InDesc.MinorVersion));
        glfwWindowHint(GLFW_OPENGL_PROFILE, InDesc.bCoreProfile ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_ANY_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, InDesc.bForwardCompatible ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, InDesc.bDebugContext ? GLFW_TRUE : GLFW_FALSE);
    }

    // =============================================================================
    // CTOR - DTOR
    // =============================================================================
    GlfwGLSurface::GlfwGLSurface(GLFWwindow* InWindow)
        : m_Window(InWindow)
    {
        OPAAX_CORE_ASSERT(m_Window)
    }

    // =============================================================================
    // IGLSurface
    // =============================================================================
    void GlfwGLSurface::MakeCurrent()
    {
        glfwMakeContextCurrent(m_Window);
    }

    GLProcLoader GlfwGLSurface::GetProcLoader() const
    {
        return &LoadGLProc;
    }

    void GlfwGLSurface::SwapBuffers()
    {
        glfwSwapBuffers(m_Window);
    }

    void GlfwGLSurface::SetSwapInterval(const Uint32 InRefreshes)
    {
        glfwSwapInterval(static_cast<int>(InRefreshes));
    }
}
