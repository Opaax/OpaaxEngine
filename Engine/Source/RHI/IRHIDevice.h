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
    // IRHIDevice — the graphics device the renderer owns (one instance, created
    //   by RHIDevice::Create for the selected backend). It knows its own backend, so it
    //   creates resources directly. One object carries both resource creation and the frame
    //   lifecycle; present lives HERE (Present -> the surface swap), never in a window class.
    //
    //   Resources are our existing TUniquePtr<I*> objects (the handle/pool DOD model is a
    //   later addition). Frame recording still goes through ICommandBuffer.
    // =============================================================================
    class OPAAX_API IRHIDevice
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
         * bring the device up against the already-created surface (context).
         * @param InSurface
         */
        virtual void Init(IGraphicsContext& InSurface) = 0;

        // =============================================================================
        // Resource creation — plain desc in, owning resource out.
        // =============================================================================
    public:
        virtual TUniquePtr<IVertexArray>   CreateVertexArray()                                           = 0;
        virtual TUniquePtr<IVertexBuffer>  CreateVertexBuffer(Uint32 InSizeBytes)                        = 0;
        virtual TUniquePtr<IIndexBuffer>   CreateIndexBuffer(const Uint32* InIndices, Uint32 InCount)    = 0;
        virtual TUniquePtr<IUniformBuffer> CreateUniformBuffer(Uint32 InSizeBytes, Uint32 InBinding)     = 0;
        virtual TUniquePtr<ITexture2D>     CreateTexture(Uint32 InWidth, Uint32 InHeight)                = 0;

        /**
         * A texture from PIXELS the caller already decoded. Raw arguments rather than an image
         * struct, like CreateIndexBuffer beside it — the device takes bytes, so it never learns
         * what a file is and a second backend inherits the decode instead of repeating it.
         *
         * @param InPixels Tightly packed rows, InWidth * InHeight * InChannels bytes. Borrowed:
         *   the call copies to the GPU and the caller may free it on return.
         * @param InChannels 4 = RGBA8, 3 = RGB8, 1 = R8 coverage (swizzled into alpha).
         */
        virtual TUniquePtr<ITexture2D>     CreateTexture(const void* InPixels, Uint32 InWidth,
                                                         Uint32 InHeight, Int32 InChannels)             = 0;
        virtual TUniquePtr<IShader>        CreateShader(const ShaderDesc& InDesc)                        = 0;
        virtual TUniquePtr<IPipeline>      CreatePipeline(const PipelineDesc& InDesc)                    = 0;
        virtual TUniquePtr<IBindGroup>     CreateBindGroup(const BindGroupLayout& InLayout)              = 0;

        /**
         * An offscreen render target's backing store. Device-owned like every other GPU resource
         * (F2a) — there is no free IFramebuffer::Create. The caller owns the returned framebuffer
         * and must release it while the GPU context is still alive.
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

        /** Present interval on the surface: true = wait for vblank. On by default (the context's Init). */
        virtual void            SetVSync(bool InEnabled)                                        = 0;
        virtual void            SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)    = 0;
        virtual void            Resize(Uint32 InWidth, Uint32 InHeight)                         = 0;
        virtual void            WaitIdle()                                                      = 0;
    };
}
