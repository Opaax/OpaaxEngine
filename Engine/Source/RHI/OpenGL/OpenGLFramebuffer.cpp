#include "OpenGLFramebuffer.h"

#include "Core/Log/OpaaxLog.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    // =============================================================================
    // IFramebuffer factory
    // =============================================================================
    UniquePtr<IFramebuffer> IFramebuffer::Create(const FramebufferSpec& InSpec)
    {
        return MakeUnique<OpenGLFramebuffer>(InSpec);
    }

    // =============================================================================
    // CTOR - DTOR
    // =============================================================================
    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpec& InSpec)
        : m_Width(InSpec.Width  ? InSpec.Width  : 1u)   // guard a zero-size first frame
        , m_Height(InSpec.Height ? InSpec.Height : 1u)
        , m_DepthStencil(InSpec.DepthStencil)
    {
        Invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        Release();
    }

    // =============================================================================
    // Function
    // =============================================================================
    void OpenGLFramebuffer::Release()
    {
        if (m_ColorTexture) { glDeleteTextures(1, &m_ColorTexture);  m_ColorTexture = 0; }
        if (m_DepthRBO)     { glDeleteRenderbuffers(1, &m_DepthRBO); m_DepthRBO     = 0; }
        if (m_FBO)          { glDeleteFramebuffers(1, &m_FBO);       m_FBO          = 0; }
    }

    void OpenGLFramebuffer::Invalidate()
    {
        glGenTextures(1, &m_ColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height),
            0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (m_DepthStencil)
        {
            glGenRenderbuffers(1, &m_DepthRBO);
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                static_cast<GLsizei>(m_Width),
                static_cast<GLsizei>(m_Height));
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_ColorTexture, 0);
        if (m_DepthStencil)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                GL_RENDERBUFFER, m_DepthRBO);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            OPAAX_CORE_ERROR("OpenGLFramebuffer: FBO incomplete at {}x{}!", m_Width, m_Height);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        // NOTE: Viewport must match the FBO dimensions, not the GLFW window — a window-sized
        //   viewport over a differently-sized FBO was the historical deformed-sprite bug.
        glViewport(0, 0,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height));
    }

    void OpenGLFramebuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        if (InWidth == 0 || InHeight == 0)                  { return; } // ignore degenerate sizes
        if (InWidth == m_Width && InHeight == m_Height && m_FBO) { return; } // no-op

        m_Width  = InWidth;
        m_Height = InHeight;

        Release();
        Invalidate();
    }
}
