#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "RHI/RenderLog.h"
#include "RHI/Buffer.h"
#include "RHI/Texture.h"
#include "RHI/Shader.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"

namespace Opaax
{
    class IGraphicsContext;
    class ICommandBuffer;

    // =============================================================================
    // IRHIDevice — the graphics device the portable renderer owns (one instance, created
    //   by RHIDevice::Create for the selected backend). It knows its own backend, so it
    //   creates resources directly. One object carries both resource creation and the frame
    //   lifecycle; present lives HERE (Present -> the surface swap), never in a window class.
    //
    //   Resources are our existing UniquePtr<I*> objects (the handle/pool DOD model is a
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
         * ring the device up against the already-created surface (context).
         * InLog is the injected sink — the device never calls the engine's OPAAX_LOG.
         * @param InSurface 
         * @param InLog 
         */
        virtual void Init(IGraphicsContext& InSurface, RenderLogFn InLog) = 0;

        // =============================================================================
        // Resource creation — plain desc in, owning resource out.
        // =============================================================================
    public:
        virtual UniquePtr<IVertexArray>   CreateVertexArray()                                           = 0;
        virtual UniquePtr<IVertexBuffer>  CreateVertexBuffer(Uint32 InSizeBytes)                        = 0;
        virtual UniquePtr<IIndexBuffer>   CreateIndexBuffer(const Uint32* InIndices, Uint32 InCount)    = 0;
        virtual UniquePtr<IUniformBuffer> CreateUniformBuffer(Uint32 InSizeBytes, Uint32 InBinding)     = 0;
        virtual UniquePtr<ITexture2D>     CreateTexture(Uint32 InWidth, Uint32 InHeight)                = 0;
        virtual UniquePtr<IShader>        CreateShader(const ShaderDesc& InDesc)                        = 0;
        virtual UniquePtr<IPipeline>      CreatePipeline(const PipelineDesc& InDesc)                    = 0;
        virtual UniquePtr<IBindGroup>     CreateBindGroup(const BindGroupLayout& InLayout)              = 0;

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
    };
}
