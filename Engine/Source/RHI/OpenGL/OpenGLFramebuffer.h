#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(OpenGLFramebuffer);
    
    /**
     * OpenGL IFramebuffer: one GL_RGBA8 colour texture and an optional GL_DEPTH24_STENCIL8 renderbuffer.
     */
    class OpenGLFramebuffer final : public IFramebuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit OpenGLFramebuffer(const FramebufferSpec& InSpec);
        ~OpenGLFramebuffer() override;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        OpenGLFramebuffer(const OpenGLFramebuffer&)            = delete;
        OpenGLFramebuffer& operator=(const OpenGLFramebuffer&) = delete;

        // =============================================================================
        // Function
        // =============================================================================
    private:
        // (Re)creates the colour texture, depth renderbuffer and FBO at the current size.
        void Invalidate();
        // Deletes the GL objects (safe to call twice).
        void Release();

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IFramebuffer interface
    public:
        void Bind()   override;
        void Unbind() override;
        void Resize(Uint32 InWidth, Uint32 InHeight) override;

        Uint32 GetColorAttachmentID() const noexcept override { return m_ColorTexture; }
        Uint32 GetWidth()             const noexcept override { return m_Width;        }
        Uint32 GetHeight()            const noexcept override { return m_Height;       }
        //~End IFramebuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_FBO          = 0;
        Uint32 m_ColorTexture = 0;
        Uint32 m_DepthRBO     = 0;
        Uint32 m_Width        = 1;
        Uint32 m_Height       = 1;
        bool   m_DepthStencil = true;
    };
}
