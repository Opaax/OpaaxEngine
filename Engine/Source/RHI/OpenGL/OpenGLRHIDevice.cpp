#include "OpenGLRHIDevice.h"

#include "Application/Services/ILogger.h"

#include "RHI/RHIDevice.h"
#include "RHI/IGraphicsContext.h"

#include "RHI/OpenGL/OpenGLVertexArray.h"
#include "RHI/OpenGL/OpenGLBuffer.h"
#include "RHI/OpenGL/OpenGLUniformBuffer.h"
#include "RHI/OpenGL/OpenGLTexture2D.h"
#include "RHI/OpenGL/OpenGLShader.h"
#include "RHI/OpenGL/OpenGLPipeline.h"
#include "RHI/OpenGL/OpenGLBindGroup.h"
#include "RHI/OpenGL/OpenGLFramebuffer.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(OpenGLRHIDevice);

    // =========================================================================
    // Lifecycle
    // =========================================================================
    void OpenGLRHIDevice::Init(IGraphicsContext& InSurface)
    {
        m_Surface = &InSurface;
        // OpenGL state is global — the surface's context is already current (window created it).
        OPAAX_LOG(LogOpenGLRHIDevice, Info, "OpenGL RHI device initialized");
    }

    // =========================================================================
    // Resource creation — reuse the existing OpenGL* impls (no GetBackend query).
    // =========================================================================
    TUniquePtr<IVertexArray>   OpenGLRHIDevice::CreateVertexArray()                        { return MakeUnique<OpenGLVertexArray>(); }
    TUniquePtr<IVertexBuffer>  OpenGLRHIDevice::CreateVertexBuffer(Uint32 InSizeBytes)     { return MakeUnique<OpenGLVertexBuffer>(InSizeBytes); }
    TUniquePtr<IIndexBuffer>   OpenGLRHIDevice::CreateIndexBuffer(const Uint32* InIndices, Uint32 InCount) { return MakeUnique<OpenGLIndexBuffer>(InIndices, InCount); }
    TUniquePtr<IUniformBuffer> OpenGLRHIDevice::CreateUniformBuffer(Uint32 InSizeBytes, Uint32 InBinding)  { return MakeUnique<OpenGLUniformBuffer>(InSizeBytes, InBinding); }
    TUniquePtr<ITexture2D>     OpenGLRHIDevice::CreateTexture(Uint32 InWidth, Uint32 InHeight) { return MakeUnique<OpenGLTexture2D>(InWidth, InHeight); }

    TUniquePtr<ITexture2D>     OpenGLRHIDevice::CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        return MakeUnique<OpenGLTexture2D>(static_cast<const unsigned char*>(InPixels), InWidth, InHeight, InChannels);
    }

    TUniquePtr<IShader>        OpenGLRHIDevice::CreateShader(const ShaderDesc& InDesc)      { return MakeUnique<OpenGLShader>(InDesc); }
    TUniquePtr<IPipeline>      OpenGLRHIDevice::CreatePipeline(const PipelineDesc& InDesc)  { return MakeUnique<OpenGLPipeline>(InDesc); }
    TUniquePtr<IBindGroup>     OpenGLRHIDevice::CreateBindGroup(const BindGroupLayout& InLayout) { return MakeUnique<OpenGLBindGroup>(InLayout); }
    TUniquePtr<IFramebuffer>   OpenGLRHIDevice::CreateFramebuffer(const FramebufferSpec& InSpec) { return MakeUnique<OpenGLFramebuffer>(InSpec); }

    // =========================================================================
    // Frame
    // =========================================================================
    void OpenGLRHIDevice::BeginFrame() {}                              // GL executes immediately — nothing to begin
    ICommandBuffer& OpenGLRHIDevice::GetCommandBuffer() { return m_CommandBuffer; }
    void OpenGLRHIDevice::EndFrame()   {}                              // nothing to flush before the swap

    void OpenGLRHIDevice::Present()
    {
        if (m_Surface) { m_Surface->SwapBuffers(); }                   // present lives on the device, not the window
    }

    void OpenGLRHIDevice::SetVSync(bool InEnabled)
    {
        if (m_Surface) { m_Surface->SetVSync(InEnabled); }
    }

    void OpenGLRHIDevice::SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)
    {
        glViewport(static_cast<GLint>(X), static_cast<GLint>(Y),
                   static_cast<GLsizei>(Width), static_cast<GLsizei>(Height));
    }

    void OpenGLRHIDevice::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        glViewport(0, 0, static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight));
    }

    void OpenGLRHIDevice::WaitIdle() { glFinish(); }

    // =========================================================================
    // Factory — OpenGL only for now. When the Vulkan device lands, this moves to a
    // neutral TU (like BackendFactory) that knows every backend.
    // =========================================================================
    TUniquePtr<IRHIDevice> RHIDevice::Create(EBackend InBackend, IGraphicsContext& InSurface)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
            {
                TUniquePtr<IRHIDevice> lDevice = MakeUnique<OpenGLRHIDevice>();
                lDevice->Init(InSurface);
                return lDevice;
            }
            default: break;
        }

        OPAAX_LOG(LogOpenGLRHIDevice, Error, "RHIDevice::Create — backend not available (only OpenGL for now).");
        return nullptr;
    }
}
