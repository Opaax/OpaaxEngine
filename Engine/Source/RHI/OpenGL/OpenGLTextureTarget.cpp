#include "RHI/OpenGL/OpenGLTextureTarget.h"

// Logger.h before glad.h: through spdlog it includes windows.h, whose APIENTRY must come first.
#include "Core/Log/Logger.h"
#include "RHI/OpenGL/OpenGLFramebuffer.h"   // ReadFramebufferPixels, LogOpenGLFramebuffer

#include <glad/glad.h>

namespace Opaax
{
    OpenGLTextureTarget::OpenGLTextureTarget(const ITexture2D& InTexture)
        : m_Texture(&InTexture)
    {
        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, InTexture.GetRendererID(), 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            OPAAX_LOG(LogOpenGLFramebuffer, Error, "A texture cannot be drawn into: its framebuffer is incomplete ({}x{})",
                      InTexture.GetWidth(), InTexture.GetHeight());
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    OpenGLTextureTarget::~OpenGLTextureTarget()
    {
        if (m_FBO != 0)
        {
            glDeleteFramebuffers(1, &m_FBO);
        }
    }

    void OpenGLTextureTarget::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, static_cast<GLsizei>(GetWidth()), static_cast<GLsizei>(GetHeight()));
    }

    void OpenGLTextureTarget::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLTextureTarget::Resize(const Uint32 InWidth, const Uint32 InHeight)
    {
        OPAAX_LOG(LogOpenGLFramebuffer, Warn, "A texture's target keeps the texture's size ({}x{}), not {}x{}",
                  GetWidth(), GetHeight(), InWidth, InHeight);
    }

    void OpenGLTextureTarget::BindColorTexture(const Uint32 InSlot) const
    {
        m_Texture->Bind(InSlot);
    }

    Uint32 OpenGLTextureTarget::GetColorAttachmentID() const noexcept
    {
        return m_Texture->GetRendererID();
    }

    Uint32 OpenGLTextureTarget::GetWidth() const noexcept
    {
        return m_Texture->GetWidth();
    }

    Uint32 OpenGLTextureTarget::GetHeight() const noexcept
    {
        return m_Texture->GetHeight();
    }

    bool OpenGLTextureTarget::ReadPixels(TDynArray<Uint8>& OutRGBA) const
    {
        return ReadFramebufferPixels(m_FBO, GetWidth(), GetHeight(), OutRGBA);
    }
}
