#pragma once

#include "RHI/IRHIDevice.h"
#include "RHI/OpenGL/OpenGLCommandBuffer.h"

namespace Opaax
{
    // =============================================================================
    // OpenGLRHIDevice — IRHIDevice for OpenGL. Owns the frame's command buffer; Present swaps
    //   the surface.
    // =============================================================================
    class OPAAX_API OpenGLRHIDevice final : public IRHIDevice
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        OpenGLRHIDevice() = default;

        /** Releases the timer queries (the GL context is still current here). */
        ~OpenGLRHIDevice() override;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * Reads the finished timer results, oldest first. Never blocks: stops at the first result
         * not ready. A slot still pending when reused loses its sample.
         */
        void HarvestGpuTimings();

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
        TUniquePtr<ITexture2D>     CreateTexture(const void* InPixels, Uint32 InWidth,
                                                 Uint32 InHeight, Int32 InChannels)             override;
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
        double          GetLastGpuFrameTimeMs() const                                   override { return m_LastGpuMs; }
        //~End IRHIDevice interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IGraphicsContext*   m_Surface = nullptr;
        OpenGLCommandBuffer m_CommandBuffer;

        // GPU timing: a ring of three GL_TIME_ELAPSED queries (the GPU trails the CPU by about a frame).
        // Uint32 instead of GLuint keeps glad out of this header.
        static constexpr Uint32 GPU_TIMER_COUNT = 3;

        Uint32 m_TimerQueries[GPU_TIMER_COUNT] = {};
        bool   m_TimerPending[GPU_TIMER_COUNT] = {};
        Uint32 m_TimerWrite   = 0;      // next slot to issue, and the oldest pending
        bool   m_bTimerOpen   = false;  // a query is running
        bool   m_bTimersReady = false;  // false disables timing

        double m_LastGpuMs = -1.0;      // -1 = nothing measured yet
    };
}
