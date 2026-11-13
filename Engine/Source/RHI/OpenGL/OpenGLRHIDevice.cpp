#include "RHI/OpenGL/OpenGLRHIDevice.h"

#include "Core/Memory/Memory.h"

#include "Core/Build/BuildInfo.h"
#include "Core/Log/Logger.h"

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
#include "RHI/OpenGL/OpenGLDebug.h"

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
        // The surface's context is already current.

        glGenQueries(static_cast<GLsizei>(GPU_TIMER_COUNT), m_TimerQueries);

        // No timer support: timing stays off (GetLastGpuFrameTimeMs returns -1).
        m_bTimersReady = m_TimerQueries[0] != 0;

        ReadCapabilities();

        if (Build::IsDevBuild())
        {
            m_Capabilities.ErrorChecks = OpenGLDebug::Install() ? ERHIErrorChecks::PerCall : ERHIErrorChecks::PerFrame;
        }

        OPAAX_LOG(LogOpenGLRHIDevice, Info,
                  "OpenGL RHI device initialized (GPU timing {}, errors checked: {}, up to {} samples)",
                  m_bTimersReady ? "on" : "UNAVAILABLE", ToString(m_Capabilities.ErrorChecks),
                  m_Capabilities.MaxSamples);
    }

    void OpenGLRHIDevice::ReadCapabilities()
    {
        auto lText = [](const GLenum InName) -> OpaaxString
        {
            const GLubyte* lValue = glGetString(InName);
            return OpaaxString(lValue ? reinterpret_cast<const char*>(lValue) : "unknown");
        };

        auto lNumber = [](const GLenum InName) -> Uint32
        {
            GLint lValue = 0;
            glGetIntegerv(InName, &lValue);
            return lValue > 0 ? static_cast<Uint32>(lValue) : 0;
        };

        m_Capabilities.Backend         = "OpenGL";
        m_Capabilities.Vendor          = lText(GL_VENDOR);
        m_Capabilities.Renderer        = lText(GL_RENDERER);
        m_Capabilities.Version         = lText(GL_VERSION);
        m_Capabilities.ShadingLanguage = lText(GL_SHADING_LANGUAGE_VERSION);
        m_Capabilities.MaxTextureSize  = lNumber(GL_MAX_TEXTURE_SIZE);
        m_Capabilities.MaxTextureUnits = lNumber(GL_MAX_TEXTURE_IMAGE_UNITS);

        // A framebuffer takes the least of what colour and depth allow.
        const Uint32 lColor = lNumber(GL_MAX_COLOR_TEXTURE_SAMPLES);
        const Uint32 lDepth = lNumber(GL_MAX_DEPTH_TEXTURE_SAMPLES);
        const Uint32 lAny   = lNumber(GL_MAX_SAMPLES);
        Uint32 lSamples     = lAny;
        if (lColor < lSamples) { lSamples = lColor; }
        if (lDepth < lSamples) { lSamples = lDepth; }
        m_Capabilities.MaxSamples = lSamples > 0 ? lSamples : 1;
    }

    OpenGLRHIDevice::~OpenGLRHIDevice()
    {
        if (m_bTimersReady)
        {
            glDeleteQueries(static_cast<GLsizei>(GPU_TIMER_COUNT), m_TimerQueries);
        }
    }

    // =========================================================================
    // Resource creation
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
    // GL runs immediately: only the GPU timer needs a begin.
    void OpenGLRHIDevice::BeginFrame()
    {
        // Read results before issuing, so the reused slot got its chance.
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
        if (m_Capabilities.ErrorChecks == ERHIErrorChecks::PerFrame)
        {
            OpenGLDebug::CheckErrors();
        }

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

        // Oldest first. Queries complete in order, so stop at the first one not ready.
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
                break;   // do not wait (that would stall)
            }

            GLuint64 lNanoseconds = 0;
            glGetQueryObjectui64v(m_TimerQueries[lSlot], GL_QUERY_RESULT, &lNanoseconds);

            m_LastGpuMs           = static_cast<double>(lNanoseconds) / 1.0e6;
            m_TimerPending[lSlot] = false;
        }
    }

    void OpenGLRHIDevice::Present()
    {
        if (m_Surface) { m_Surface->SwapBuffers(); }
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

    bool OpenGLRHIDevice::ReadBackbufferPixels(const Uint32 InWidth, const Uint32 InHeight, TDynArray<Uint8>& OutRGBA)
    {
        if (InWidth == 0 || InHeight == 0)
        {
            return false;
        }

        // Errors from earlier calls must not be taken for this one's.
        while (glGetError() != GL_NO_ERROR) {}

        const size_t lRowBytes = static_cast<size_t>(InWidth) * 4;
        TDynArray<Uint8> lBottomUp(lRowBytes * InHeight);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight), GL_RGBA, GL_UNSIGNED_BYTE,
                     lBottomUp.data());
        glPixelStorei(GL_PACK_ALIGNMENT, 4);

        if (glGetError() != GL_NO_ERROR)
        {
            OPAAX_LOG(LogOpenGLRHIDevice, Error, "The backbuffer could not be read ({}x{})", InWidth, InHeight);
            return false;
        }

        // GL reads from the bottom row up. What the window shows is opaque, whatever the alpha.
        OutRGBA.resize(lBottomUp.size());
        for (Uint32 lRow = 0; lRow < InHeight; ++lRow)
        {
            const Uint8* lSource = lBottomUp.data() + (InHeight - 1 - lRow) * lRowBytes;
            Uint8*       lTarget = OutRGBA.data() + lRow * lRowBytes;
            Memory::Copy(lTarget, lSource, lRowBytes);

            for (size_t lAlpha = 3; lAlpha < lRowBytes; lAlpha += 4)
            {
                lTarget[lAlpha] = 255;
            }
        }

        return true;
    }

    // =========================================================================
    // Factory — OpenGL only for now.
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
