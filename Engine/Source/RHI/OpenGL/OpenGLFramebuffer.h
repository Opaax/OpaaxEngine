#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    /**
     * @class OpenGLFramebuffer
     *
     * OpenGL IFramebuffer implementation: one GL_RGBA8 color texture + an optional
     * GL_DEPTH24_STENCIL8 renderbuffer. Holds the GL handles that previously lived
     * inline in the editor ViewportPanel.
     */
    class OPAAX_API OpenGLFramebuffer final : public IFramebuffer
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
        // (re)create the color texture + depth RBO + FBO at the current size.
        void Invalidate();
        // delete the GL objects (idempotent).
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
