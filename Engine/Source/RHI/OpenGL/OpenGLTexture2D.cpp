#include "RHI/OpenGL/OpenGLTexture2D.h"

// Logger.h first: through spdlog it includes windows.h, whose APIENTRY must come before glad.h
// (otherwise warning C4005).
#include "Core/Log/Logger.h"

#include <glad/glad.h>
#include "Core/EngineAPI.h"

namespace Opaax
{

    OpenGLTexture2D::OpenGLTexture2D(Uint32 InWidth, Uint32 InHeight)
    {
        // White pixels: multiplied by the tint in the shader.
        m_Width  = InWidth;
        m_Height = InHeight;
        const TDynArray<Uint32> lWhite(static_cast<size_t>(InWidth) * InHeight, 0xFFFFFFFFu);

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, lWhite.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_bLoaded = true;
    }

    OpenGLTexture2D::OpenGLTexture2D(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        if (!InData)
        {
            OPAAX_ENGINE_LOG(Error, "OpenGLTexture2D: raw upload received null data");
            return;
        }
        Upload(InData, InWidth, InHeight, InChannels);
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::Upload(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        m_Width  = InWidth;
        m_Height = InHeight;

        const GLenum lInternalFormat = (InChannels == 4) ? GL_RGBA8
                                     : (InChannels == 1) ? GL_R8
                                     : GL_RGB8;
        const GLenum lDataFormat     = (InChannels == 4) ? GL_RGBA
                                     : (InChannels == 1) ? GL_RED
                                     : GL_RGB;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);

        const bool lIsR8 = (InChannels == 1);
        if (lIsR8)
        {
            // Swizzle coverage into alpha, so the RGBA sprite shader reads (1,1,1,coverage).
            // LINEAR + CLAMP_TO_EDGE avoids bleeding between atlas cells.
            const GLint lSwizzle[4] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, lSwizzle);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        }

        // Tightly packed rows: a 1- or 3-byte pixel row is not a multiple of the default 4 bytes.
        const bool bTightRows = (InChannels != 4);
        if (bTightRows)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(lInternalFormat),
                     static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight), 0,
                     lDataFormat, GL_UNSIGNED_BYTE, InData);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (bTightRows)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }

        m_bLoaded = true;
    }
    
    void OpenGLTexture2D::Bind(Uint32 InSlot) const
    {
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

    void OpenGLTexture2D::Unbind() const
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
