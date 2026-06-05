// =============================================================================
// BackendFactory.cpp
// =============================================================================
// The single neutral translation unit that knows every graphics backend. All
// backend-selecting factories live here so no backend's TU ever includes another's:
//   - RenderAPI::Create / BackendFromString / BackendToString
//   - IGraphicsContext::Create / ApplyWindowHints
//   - every resource I*::Create (IVertexArray, IVertexBuffer, IIndexBuffer, ITexture2D,
//     IShader, IUniformBuffer, IPipeline, IBindGroup, IFramebuffer)
//
// Each factory dispatches on the active backend: IGraphicsContext::Create/ApplyWindowHints
// take it as a parameter; the resource factories read RenderAPI::GetBackend() (set by
// RenderAPI::Create, which always runs before any resource is created in Renderer2D::Init).
//
// Vulkan cases are wrapped #if OPAAX_HAS_VULKAN — absent the SDK, "Vulkan" selection falls
// back to OpenGL (logged).
// =============================================================================

#include "RHI/RenderAPI.h"
#include "RHI/IGraphicsContext.h"
#include "RHI/Shader.h"
#include "RHI/Texture.h"
#include "RHI/Buffer.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"
#include "RHI/Framebuffer.h"

#include "RHI/OpenGL/OpenGLRenderAPI.h"
#include "RHI/OpenGL/OpenGLContext.h"
#include "RHI/OpenGL/OpenGLShader.h"
#include "RHI/OpenGL/OpenGLTexture2D.h"
#include "RHI/OpenGL/OpenGLBuffer.h"
#include "RHI/OpenGL/OpenGLVertexArray.h"
#include "RHI/OpenGL/OpenGLUniformBuffer.h"
#include "RHI/OpenGL/OpenGLPipeline.h"
#include "RHI/OpenGL/OpenGLBindGroup.h"
#include "RHI/OpenGL/OpenGLFramebuffer.h"

#if OPAAX_HAS_VULKAN
    #include "RHI/Vulkan/VulkanContext.h"
    #include "RHI/Vulkan/VulkanRenderAPI.h"
    #include "RHI/Vulkan/VulkanShader.h"
    #include "RHI/Vulkan/VulkanTexture2D.h"
    #include "RHI/Vulkan/VulkanBuffer.h"
    #include "RHI/Vulkan/VulkanVertexArray.h"
    #include "RHI/Vulkan/VulkanUniformBuffer.h"
    #include "RHI/Vulkan/VulkanPipeline.h"
    #include "RHI/Vulkan/VulkanBindGroup.h"
    #include "RHI/Vulkan/VulkanFramebuffer.h"
#endif

#include "Core/Log/OpaaxLog.h"

#include <GLFW/glfw3.h>

namespace Opaax
{
    // =============================================================================
    // Active backend
    // =============================================================================
    EBackend RenderAPI::s_Backend = EBackend::OpenGL;

    // =============================================================================
    // RenderAPI factory + string map
    // =============================================================================
    UniquePtr<IRenderAPI> RenderAPI::Create(EBackend InBackend)
    {
        s_Backend = InBackend;   // record before any resource factory runs

        switch (InBackend)
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLRenderAPI>();
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanRenderAPI>();
#endif
            default: break;
        }

        OPAAX_CORE_ERROR("RenderAPI::Create — backend '{}' not available; no IRenderAPI created.",
                         BackendToString(InBackend));
        return nullptr;
    }

    EBackend RenderAPI::BackendFromString(const OpaaxString& InName)
    {
        if (InName == "OpenGL") { return EBackend::OpenGL; }

        if (InName == "Vulkan")
        {
#if OPAAX_HAS_VULKAN
            return EBackend::Vulkan;
#else
            OPAAX_CORE_WARN("RenderAPI: 'Vulkan' requested but the engine was built without the "
                            "Vulkan SDK — falling back to OpenGL.");
            return EBackend::OpenGL;
#endif
        }

        OPAAX_CORE_WARN("RenderAPI: unknown render backend '{}' — falling back to OpenGL.", InName);
        return EBackend::OpenGL;
    }

    const char* RenderAPI::BackendToString(EBackend InBackend) noexcept
    {
        switch (InBackend)
        {
            case EBackend::OpenGL: return "OpenGL";
            case EBackend::Vulkan: return "Vulkan";
        }
        return "Unknown";
    }

    // =============================================================================
    // IGraphicsContext factory + window hints
    // =============================================================================
    UniquePtr<IGraphicsContext> IGraphicsContext::Create(EBackend InBackend, void* InNativeWindow)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
                return MakeUnique<OpenGLContext>(static_cast<GLFWwindow*>(InNativeWindow));
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan:
                return MakeUnique<VulkanContext>(static_cast<GLFWwindow*>(InNativeWindow));
#endif
            default: break;
        }

        OPAAX_CORE_ERROR("IGraphicsContext::Create — backend not available; no context created.");
        return nullptr;
    }

    void IGraphicsContext::ApplyWindowHints(EBackend InBackend)
    {
        switch (InBackend)
        {
            // OpenGL: leave GLFW at its defaults (a GL context) — preserves prior behavior.
            case EBackend::OpenGL:
                break;

            // Vulkan: tell GLFW not to create any client API context (we own the swapchain).
            case EBackend::Vulkan:
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                break;
        }
    }

    // =============================================================================
    // Resource factories — dispatch on RenderAPI::GetBackend()
    // =============================================================================

    UniquePtr<IVertexArray> IVertexArray::Create()
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLVertexArray>();
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanVertexArray>();
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IVertexArray::Create — backend not available."); return nullptr;
    }

    UniquePtr<IVertexBuffer> IVertexBuffer::Create(Uint32 InSize)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLVertexBuffer>(InSize);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanVertexBuffer>(InSize);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IVertexBuffer::Create — backend not available."); return nullptr;
    }

    UniquePtr<IVertexBuffer> IVertexBuffer::Create(const float* InVertices, Uint32 InSize)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLVertexBuffer>(InVertices, InSize);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanVertexBuffer>(InVertices, InSize);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IVertexBuffer::Create — backend not available."); return nullptr;
    }

    UniquePtr<IIndexBuffer> IIndexBuffer::Create(const Uint32* InIndices, Uint32 InCount)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLIndexBuffer>(InIndices, InCount);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanIndexBuffer>(InIndices, InCount);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IIndexBuffer::Create — backend not available."); return nullptr;
    }

    UniquePtr<ITexture2D> ITexture2D::Create(const char* InPath)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLTexture2D>(InPath);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanTexture2D>(InPath);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("ITexture2D::Create — backend not available."); return nullptr;
    }

    UniquePtr<ITexture2D> ITexture2D::Create(Uint32 InWidth, Uint32 InHeight)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLTexture2D>(InWidth, InHeight);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanTexture2D>(InWidth, InHeight);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("ITexture2D::Create — backend not available."); return nullptr;
    }

    UniquePtr<ITexture2D> ITexture2D::Create(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLTexture2D>(InData, InWidth, InHeight, InChannels);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanTexture2D>(InData, InWidth, InHeight, InChannels);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("ITexture2D::Create — backend not available."); return nullptr;
    }

    UniquePtr<IShader> IShader::Create(const ShaderDesc& InDesc)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLShader>(InDesc);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanShader>(InDesc);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IShader::Create — backend not available."); return nullptr;
    }

    UniquePtr<IUniformBuffer> IUniformBuffer::Create(Uint32 InSize, Uint32 InBinding)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLUniformBuffer>(InSize, InBinding);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanUniformBuffer>(InSize, InBinding);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IUniformBuffer::Create — backend not available."); return nullptr;
    }

    UniquePtr<IPipeline> IPipeline::Create(const PipelineDesc& InDesc)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLPipeline>(InDesc);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanPipeline>(InDesc);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IPipeline::Create — backend not available."); return nullptr;
    }

    UniquePtr<IBindGroup> IBindGroup::Create(const BindGroupLayout& InLayout)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLBindGroup>(InLayout);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanBindGroup>(InLayout);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IBindGroup::Create — backend not available."); return nullptr;
    }

    UniquePtr<IFramebuffer> IFramebuffer::Create(const FramebufferSpec& InSpec)
    {
        switch (RenderAPI::GetBackend())
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLFramebuffer>(InSpec);
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan: return MakeUnique<VulkanFramebuffer>(InSpec);
#endif
            default: break;
        }
        OPAAX_CORE_ERROR("IFramebuffer::Create — backend not available."); return nullptr;
    }

} // namespace Opaax
