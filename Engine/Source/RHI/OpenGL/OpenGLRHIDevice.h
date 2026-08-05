#pragma once

#include "RHI/IRHIDevice.h"
#include "RHI/OpenGL/OpenGLCommandBuffer.h"

namespace Opaax
{
    // =============================================================================
    // OpenGLRHIDevice — IRHIDevice for OpenGL. A thin wrapper that reuses every existing
    //   OpenGL* resource impl + OpenGLCommandBuffer; it just knows it's OpenGL, so no
    //   backend-dispatch query is needed. Owns the frame's immediate-executing command buffer;
    //   Present forwards to the surface (IGraphicsContext::SwapBuffers).
    // =============================================================================
    class OPAAX_API OpenGLRHIDevice final : public IRHIDevice
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IRHIDevice interface
    public:
        TUniquePtr<IVertexArray>   CreateVertexArray()                                           override;
        TUniquePtr<IVertexBuffer>  CreateVertexBuffer(Uint32 InSizeBytes)                        override;
        TUniquePtr<IIndexBuffer>   CreateIndexBuffer(const Uint32* InIndices, Uint32 InCount)    override;
        TUniquePtr<IUniformBuffer> CreateUniformBuffer(Uint32 InSizeBytes, Uint32 InBinding)     override;
        TUniquePtr<ITexture2D>     CreateTexture(Uint32 InWidth, Uint32 InHeight)                override;
        TUniquePtr<IShader>        CreateShader(const ShaderDesc& InDesc)                        override;
        TUniquePtr<IPipeline>      CreatePipeline(const PipelineDesc& InDesc)                    override;
        TUniquePtr<IBindGroup>     CreateBindGroup(const BindGroupLayout& InLayout)              override;
        TUniquePtr<IFramebuffer>   CreateFramebuffer(const FramebufferSpec& InSpec)              override;

        void            Init(IGraphicsContext& InSurface)                               override;
        void            BeginFrame()                                                    override;
        ICommandBuffer& GetCommandBuffer()                                              override;
        void            EndFrame()                                                      override;
        void            Present()                                                       override;
        void            SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)    override;
        void            Resize(Uint32 InWidth, Uint32 InHeight)                         override;
        void            WaitIdle()                                                      override;
        //~End IRHIDevice interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IGraphicsContext*   m_Surface = nullptr;
        OpenGLCommandBuffer m_CommandBuffer;
    };
}
