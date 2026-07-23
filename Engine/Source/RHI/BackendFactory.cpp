// =============================================================================
// BackendFactory.cpp
// =============================================================================
// The neutral translation unit that knows the graphics backend(s). It keeps the
// backend-selecting factories in one place so no other TU has to:
//   - BackendFromString / BackendToString (config string <-> EBackend)
//   - IGraphicsContext::Create / ApplyWindowHints
//   - IFramebuffer::Create
//
// New-path status: OpenGL-only. The old IRenderAPI/RenderCommand facade and the whole
// Vulkan backend were retired to Legacy/RHI (the new path runs through IRHIDevice —
// RHIDevice::Create in OpenGLRHIDevice.cpp). When a new-path VulkanRHIDevice lands, the
// backend switch returns here.
//
// Resource creation is NOT here anymore — it lives on the device (IRHIDevice::CreateXxx).
// =============================================================================

#include "RHI/RHIBackend.h"
#include "RHI/IGraphicsContext.h"
#include "RHI/Framebuffer.h"

#include "RHI/OpenGL/OpenGLContext.h"
#include "RHI/OpenGL/OpenGLFramebuffer.h"

#include "Application/Services/ILogger.h"

#include <GLFW/glfw3.h>

namespace Opaax
{
    // =============================================================================
    // Backend string mapping
    // =============================================================================
    EBackend BackendFromString(const OpaaxString& InName)
    {
        if (InName == "OpenGL") { return EBackend::OpenGL; }

        if (InName == "Vulkan")
        {
            OPAAX_ENGINE_LOG(Warn, "RHI: 'Vulkan' requested but the Vulkan backend is parked in Legacy "
                             "(new path is OpenGL-only) — falling back to OpenGL.");
            return EBackend::OpenGL;
        }

        OPAAX_ENGINE_LOG(Warn, "RHI: unknown render backend '{}' — falling back to OpenGL.", InName);
        return EBackend::OpenGL;
    }

    const char* BackendToString(EBackend InBackend) noexcept
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
    UniquePtr<IGraphicsContext> IGraphicsContext::Create(EBackend InBackend, void* InNativeWindow)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
                return MakeUnique<OpenGLContext>(static_cast<GLFWwindow*>(InNativeWindow));
            default: break;
        }

        OPAAX_ENGINE_LOG(Error, "IGraphicsContext::Create — backend '{}' not available on the new path; "
                         "no context created.", BackendToString(InBackend));
        return nullptr;
    }

    void IGraphicsContext::ApplyWindowHints(EBackend InBackend)
    {
        switch (InBackend)
        {
            // OpenGL: leave GLFW at its defaults (a GL context) — preserves prior behavior.
            case EBackend::OpenGL:
            default:
                break;
        }
    }

    // =============================================================================
    // IFramebuffer factory (OpenGL only)
    // =============================================================================
    UniquePtr<IFramebuffer> IFramebuffer::Create(const FramebufferSpec& InSpec)
    {
        return MakeUnique<OpenGLFramebuffer>(InSpec);
    }

} // namespace Opaax
