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

        glGenQueries(static_cast<GLsizei>(GPU_TIMER_COUNT), m_TimerQueries);

        // A driver with no timer support leaves the names at 0; issuing against one would be a GL
        // error every frame, so timing simply stays off and GetLastGpuFrameTimeMs answers -1.
        m_bTimersReady = m_TimerQueries[0] != 0;

        OPAAX_LOG(LogOpenGLRHIDevice, Info, "OpenGL RHI device initialized (GPU timing {})",
                  m_bTimersReady ? "on" : "UNAVAILABLE");
    }

    OpenGLRHIDevice::~OpenGLRHIDevice()
    {
        if (m_bTimersReady)
        {
            glDeleteQueries(static_cast<GLsizei>(GPU_TIMER_COUNT), m_TimerQueries);
        }
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
    // GL executes immediately, so there is nothing to BEGIN — except the frame's GPU timer, which
    // is exactly what this bracket is for (④ S3).
    void OpenGLRHIDevice::BeginFrame()
    {
        // Read results BEFORE issuing, so the slot about to be reused has had its last chance.
        HarvestGpuTimings();

        if (m_bTimersReady)
        {
            glBeginQuery(GL_TIME_ELAPSED, m_TimerQueries[m_TimerWrite]);
            m_bTimerOpen = true;
        }
    }

    ICommandBuffer& OpenGLRHIDevice::GetCommandBuffer() { return m_CommandBuffer; }

    void OpenGLRHIDevice::EndFrame()
    {
        // Nothing to flush before the swap; only the timer closes here.
        if (!m_bTimerOpen)
        {
            return;
        }

        glEndQuery(GL_TIME_ELAPSED);

        m_TimerPending[m_TimerWrite] = true;
        m_TimerWrite                 = (m_TimerWrite + 1) % GPU_TIMER_COUNT;
        m_bTimerOpen                 = false;
    }

    void OpenGLRHIDevice::HarvestGpuTimings()
    {
        if (!m_bTimersReady)
        {
            return;
        }

        // Oldest first — m_TimerWrite is both the next slot to issue into and the longest-pending
        // one. Queries complete in submission order, so the first that is not ready ends the sweep.
        for (Uint32 i = 0; i < GPU_TIMER_COUNT; ++i)
        {
            const Uint32 lSlot = (m_TimerWrite + i) % GPU_TIMER_COUNT;

            if (!m_TimerPending[lSlot])
            {
                continue;
            }

            GLint lAvailable = 0;
            glGetQueryObjectiv(m_TimerQueries[lSlot], GL_QUERY_RESULT_AVAILABLE, &lAvailable);

            if (lAvailable == GL_FALSE)
            {
                break;   // and NOT a wait — blocking here is the stall this tool exists to expose
            }

            GLuint64 lNanoseconds = 0;
            glGetQueryObjectui64v(m_TimerQueries[lSlot], GL_QUERY_RESULT, &lNanoseconds);

            m_LastGpuMs           = static_cast<double>(lNanoseconds) / 1.0e6;
            m_TimerPending[lSlot] = false;
        }
    }

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
