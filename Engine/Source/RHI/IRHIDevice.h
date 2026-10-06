#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "RHI/Buffer.h"
#include "RHI/Texture.h"
#include "RHI/Shader.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    class IGraphicsContext;
    class ICommandBuffer;

    // =============================================================================
    // IRHIDevice — the graphics device (created by RHIDevice::Create). Creates GPU resources
    //   and runs the frame, including Present.
    // =============================================================================
    class IRHIDevice
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IRHIDevice() = default;

        // =============================================================================
        // Lifecycle
        // =============================================================================
    public:
        /**
         * Starts the device on an existing surface (context).
         */
        virtual void Init(IGraphicsContext& InSurface) = 0;

        // =============================================================================
        // Resource creation
        // =============================================================================
    public:
        virtual TUniquePtr<IVertexArray>   CreateVertexArray()                                           = 0;
        virtual TUniquePtr<IVertexBuffer>  CreateVertexBuffer(Uint32 InSizeBytes)                        = 0;
        virtual TUniquePtr<IIndexBuffer>   CreateIndexBuffer(const Uint32* InIndices, Uint32 InCount)    = 0;
        virtual TUniquePtr<IUniformBuffer> CreateUniformBuffer(Uint32 InSizeBytes, Uint32 InBinding)     = 0;
        virtual TUniquePtr<ITexture2D>     CreateTexture(Uint32 InWidth, Uint32 InHeight)                = 0;

        /**
         * Creates a texture from decoded pixels.
         * @param InPixels Tightly packed, InWidth * InHeight * InChannels bytes. Copied.
         * @param InChannels 4 = RGBA8, 3 = RGB8, 1 = R8 (swizzled into alpha)
         */
        virtual TUniquePtr<ITexture2D>     CreateTexture(const void* InPixels, Uint32 InWidth,
                                                         Uint32 InHeight, Int32 InChannels)             = 0;
        virtual TUniquePtr<IShader>        CreateShader(const ShaderDesc& InDesc)                        = 0;
        virtual TUniquePtr<IPipeline>      CreatePipeline(const PipelineDesc& InDesc)                    = 0;
        virtual TUniquePtr<IBindGroup>     CreateBindGroup(const BindGroupLayout& InLayout)              = 0;

        /**
         * Creates an offscreen framebuffer. Release it while the GPU context is alive.
         */
        virtual TUniquePtr<IFramebuffer>   CreateFramebuffer(const FramebufferSpec& InSpec)             = 0;

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        virtual void            BeginFrame()                                                    = 0;
        virtual ICommandBuffer& GetCommandBuffer()                                              = 0;
        virtual void            EndFrame()                                                      = 0;
        virtual void            Present()                                                       = 0;
        virtual void            SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)    = 0;
        virtual void            Resize(Uint32 InWidth, Uint32 InHeight)                         = 0;
        virtual void            WaitIdle()                                                      = 0;

        /**
         * GPU time of a recent frame, in milliseconds (measured between BeginFrame and EndFrame).
         * @return The latest available result (1-2 frames old, to avoid stalling); -1 before the
         *   first result or without timer support
         */
        virtual double          GetLastGpuFrameTimeMs() const                                   = 0;
    };
}
