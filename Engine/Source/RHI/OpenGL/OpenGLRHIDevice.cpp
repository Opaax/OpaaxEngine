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

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(OpenGLRHIDevice)

    // =========================================================================
    // Lifecycle
    // =========================================================================
    void OpenGLRHIDevice::Init(IGraphicsContext& InSurface)
    {
        m_Surface = &InSurface;
        // OpenGL state is global — the surface's context is already current (window created it).
        OPAAX_LOG(LogOpenGLRHIDevice, Info, "OpenGL RHI device initialized")
    }

    // =========================================================================
    // Resource creation — reuse the existing OpenGL* impls (no GetBackend query).
    // =========================================================================
    UniquePtr<IVertexArray>   OpenGLRHIDevice::CreateVertexArray()                        { return MakeUnique<OpenGLVertexArray>(); }
    UniquePtr<IVertexBuffer>  OpenGLRHIDevice::CreateVertexBuffer(Uint32 InSizeBytes)     { return MakeUnique<OpenGLVertexBuffer>(InSizeBytes); }
    UniquePtr<IIndexBuffer>   OpenGLRHIDevice::CreateIndexBuffer(const Uint32* InIndices, Uint32 InCount) { return MakeUnique<OpenGLIndexBuffer>(InIndices, InCount); }
    UniquePtr<IUniformBuffer> OpenGLRHIDevice::CreateUniformBuffer(Uint32 InSizeBytes, Uint32 InBinding)  { return MakeUnique<OpenGLUniformBuffer>(InSizeBytes, InBinding); }
    UniquePtr<ITexture2D>     OpenGLRHIDevice::CreateTexture(Uint32 InWidth, Uint32 InHeight) { return MakeUnique<OpenGLTexture2D>(InWidth, InHeight); }
    UniquePtr<IShader>        OpenGLRHIDevice::CreateShader(const ShaderDesc& InDesc)      { return MakeUnique<OpenGLShader>(InDesc); }
    UniquePtr<IPipeline>      OpenGLRHIDevice::CreatePipeline(const PipelineDesc& InDesc)  { return MakeUnique<OpenGLPipeline>(InDesc); }
    UniquePtr<IBindGroup>     OpenGLRHIDevice::CreateBindGroup(const BindGroupLayout& InLayout) { return MakeUnique<OpenGLBindGroup>(InLayout); }

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
    UniquePtr<IRHIDevice> RHIDevice::Create(EBackend InBackend, IGraphicsContext& InSurface)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
            {
                UniquePtr<IRHIDevice> lDevice = MakeUnique<OpenGLRHIDevice>();
                lDevice->Init(InSurface);
                return lDevice;
            }
            default: break;
        }

        OPAAX_LOG(LogOpenGLRHIDevice, Error, "RHIDevice::Create — backend not available (only OpenGL for now).")
        return nullptr;
    }
}
