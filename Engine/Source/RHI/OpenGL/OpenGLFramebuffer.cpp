#include "RHI/OpenGL/OpenGLFramebuffer.h"

#include <glad/glad.h>

#include "Core/Log/Logger.h"

namespace Opaax
{

    // =============================================================================
    // CTOR - DTOR
    // =============================================================================
    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpec& InSpec)
        : m_Width(InSpec.Width  ? InSpec.Width  : 1u)   // avoid a zero size
        , m_Height(InSpec.Height ? InSpec.Height : 1u)
        , m_DepthStencil(InSpec.DepthStencil)
        , m_Format(InSpec.ColorFormat)
        , m_bLinearFilter(InSpec.bLinearFilter)
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
        GLint  lInternal = GL_RGBA8;
        GLenum lFormat   = GL_RGBA;
        GLenum lType     = GL_UNSIGNED_BYTE;
        switch (m_Format)
        {
            case ETextureFormat::RGBA8:   lInternal = GL_RGBA8;   lFormat = GL_RGBA; lType = GL_UNSIGNED_BYTE; break;
            case ETextureFormat::RGBA16F: lInternal = GL_RGBA16F; lFormat = GL_RGBA; lType = GL_HALF_FLOAT;    break;
            case ETextureFormat::R8:      lInternal = GL_R8;      lFormat = GL_RED;  lType = GL_UNSIGNED_BYTE; break;
            case ETextureFormat::R16F:    lInternal = GL_R16F;    lFormat = GL_RED;  lType = GL_HALF_FLOAT;    break;
        }

        const GLint lFilter = m_bLinearFilter ? GL_LINEAR : GL_NEAREST;

        glGenTextures(1, &m_ColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, lInternal,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height),
            0, lFormat, lType, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, lFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, lFilter);

        // Sampled by post-process passes: reads past the edge repeat the edge, never wrap around.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
            OPAAX_LOG(LogOpenGLFramebuffer, Error, "FBO incomplete at {}x{}!", m_Width, m_Height);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        // The viewport must match the framebuffer size, not the window.
        glViewport(0, 0,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height));
    }

    void OpenGLFramebuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::BindColorTexture(const Uint32 InSlot) const
    {
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
    }

    void OpenGLFramebuffer::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        if (InWidth == 0 || InHeight == 0)                  { return; } // ignore zero sizes
        if (InWidth == m_Width && InHeight == m_Height && m_FBO) { return; } // unchanged

        m_Width  = InWidth;
        m_Height = InHeight;

        Release();
        Invalidate();
    }
}
