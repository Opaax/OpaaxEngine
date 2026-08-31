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
        // CTORS - DTORS
        // =============================================================================
    public:
        OpenGLRHIDevice() = default;

        /** Releases the timer queries. Safe here: RenderSystem::Shutdown drops the device while the
         *  GL context is still current, and these are the DEVICE's own objects, not a caller's. */
        ~OpenGLRHIDevice() override;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * Read back whatever timer results the GPU has finished with, oldest first.
         *
         * NEVER blocks: it asks GL_QUERY_RESULT_AVAILABLE and stops at the first one that is not
         * ready, because queries complete in submission order. A slot still pending when its turn
         * to be reused comes round simply loses that sample — dropping one reading is cheaper than
         * the pipeline stall that waiting for it would cost.
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

        // GPU timing (④ S3) — a small ring of GL_TIME_ELAPSED queries. THREE deep: the GPU trails
        // the CPU by about a frame, so one would always be read too early and two leaves no slack
        // for a hitch. Uint32 rather than GLuint so glad stays out of this header.
        static constexpr Uint32 GPU_TIMER_COUNT = 3;

        Uint32 m_TimerQueries[GPU_TIMER_COUNT] = {};
        bool   m_TimerPending[GPU_TIMER_COUNT] = {};
        Uint32 m_TimerWrite   = 0;      // next slot to issue into, and the oldest pending one
        bool   m_bTimerOpen   = false;  // a glBeginQuery is outstanding — only one may be
        bool   m_bTimersReady = false;  // the queries were created; false disables timing entirely

        double m_LastGpuMs = -1.0;      // -1 = nothing measured yet (see IRHIDevice)
    };
}
