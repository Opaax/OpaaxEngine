// =============================================================================
// BackendFactory.cpp — the backend-specific factories: backend names and availability,
// IGraphicsContext::Create and ApplyWindowHints. OpenGL is the only backend.
// Resources are created by the device (IRHIDevice::CreateXxx); the context is created here
// because it must exist before the device.
// =============================================================================

#include "RHI/RHIBackend.h"
#include "RHI/IGraphicsContext.h"

#include "RHI/OpenGL/OpenGLContext.h"

#include "Core/Build/BuildInfo.h"
#include "Core/Log/Logger.h"

#include <GLFW/glfw3.h>

namespace Opaax
{
    // =============================================================================
    // Backend string mapping
    // =============================================================================
    EBackend ResolveSupportedBackend(const EBackend InRequested)
    {
        if (InRequested == EBackend::Vulkan)
        {
            OPAAX_ENGINE_LOG(Warn, "RHI: 'Vulkan' is asked for, and the engine has no Vulkan backend: OpenGL is "
                             "used. Set Render.Backend to OpenGL in the engine config to drop this line.");
            return EBackend::OpenGL;
        }

        return InRequested;
    }

    const char* ToString(EBackend InBackend) noexcept
    {
        switch (InBackend)
        {
            case EBackend::OpenGL: return "OpenGL";
            case EBackend::Vulkan: return "Vulkan";
        }
        return "Unknown";
    }

    // =============================================================================
    // IGraphicsContext factory + window hints (OpenGL only)
    // =============================================================================
    TUniquePtr<IGraphicsContext> IGraphicsContext::Create(EBackend InBackend, void* InNativeWindow)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
                return MakeUnique<OpenGLContext>(static_cast<GLFWwindow*>(InNativeWindow));
            default: break;
        }

        OPAAX_ENGINE_LOG(Error, "IGraphicsContext::Create: the engine has no '{}' backend, so no context is "
                         "created. OpenGL is the one it has.", ToString(InBackend));
        return nullptr;
    }

    void IGraphicsContext::ApplyWindowHints(EBackend InBackend)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
            default:
                // OpenGL 4.1 core everywhere: the most macOS offers, so every platform runs the same
                // code (shaders are ported by GLSLPort). Forward compatible, as macOS requires.
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

                // A debug context lets the driver say which call is wrong (OpenGLDebug). Dev builds
                // only: it costs some speed.
                glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, Build::IsDevBuild() ? GLFW_TRUE : GLFW_FALSE);
                break;
        }
    }

} // namespace Opaax
