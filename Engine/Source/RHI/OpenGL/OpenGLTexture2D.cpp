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

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_bLoaded = true;
    }

    namespace
    {
        GLenum DataFormatOf(const Int32 InChannels)
        {
            return (InChannels == 4) ? GL_RGBA : (InChannels == 1) ? GL_RED : GL_RGB;
        }
    }

    OpenGLTexture2D::OpenGLTexture2D(const TextureDesc& InDesc, const unsigned char* InData,
                                     const Uint32 InProjectSampler, Uint32* InWritableCount)
    {
        // A coverage atlas, or a texture asked smooth, keeps its own sampling, whatever the project's filter.
        const bool lOwnSampling = (InDesc.Channels == 1) || InDesc.bSmooth;
        m_SamplerID = lOwnSampling ? 0 : InProjectSampler;
        m_Channels  = InDesc.Channels;

        if (!InData)
        {
            OPAAX_ENGINE_LOG(Error, "OpenGLTexture2D: raw upload received null data");
            return;
        }
        Upload(InData, InDesc.Width, InDesc.Height, InDesc.Channels, InDesc.bSmooth);

        if (InWritableCount != nullptr)
        {
            m_WritableCount = InWritableCount;
            ++*m_WritableCount;
        }
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        glDeleteTextures(1, &m_RendererID);

        if (m_WritableCount != nullptr)
        {
            --*m_WritableCount;
        }
    }

    void OpenGLTexture2D::Upload(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels,
                                 const bool bInSmooth)
    {
        m_Width  = InWidth;
        m_Height = InHeight;

        const GLenum lInternalFormat = (InChannels == 4) ? GL_RGBA8
                                     : (InChannels == 1) ? GL_R8
                                     : GL_RGB8;
        const GLenum lDataFormat     = DataFormatOf(InChannels);

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        // The texture's own parameters: what is read where no sampler object is bound (the
        // editor's previews). Clamped: an edge takes nothing from the opposite side.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

        const bool lIsR8 = (InChannels == 1);
        if (lIsR8)
        {
            // Swizzle coverage into alpha, so the RGBA sprite shader reads (1,1,1,coverage).
            // Blended when enlarged too: glyphs are drawn at any size.
            const GLint lSwizzle[4] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, lSwizzle);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else if (bInSmooth)
        {
            // Read with these parameters, not the project's sampler: blended at any size.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

        // Made for every colour texture, whatever the filter now: the filter can change while
        // the game runs, and only the Smooth one reads them.
        if (m_SamplerID != 0)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        if (bTightRows)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }

        m_bLoaded = true;
    }

    bool OpenGLTexture2D::Update(const TextureRegion& InRegion, const void* InPixels)
    {
        if (m_WritableCount == nullptr || !m_bLoaded || InPixels == nullptr || !IsRegionInside(InRegion, m_Width, m_Height))
        {
            return false;
        }

        const bool bTightRows = (m_Channels != 4);
        if (bTightRows)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }

        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        static_cast<GLint>(InRegion.X), static_cast<GLint>(InRegion.Y),
                        static_cast<GLsizei>(InRegion.Width), static_cast<GLsizei>(InRegion.Height),
                        DataFormatOf(m_Channels), GL_UNSIGNED_BYTE, InPixels);

        // The smaller levels show the old pixels until made again.
        if (m_SamplerID != 0)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        if (bTightRows)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }

        return true;
    }

    void OpenGLTexture2D::Bind(Uint32 InSlot) const
    {
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        // A sampler object stays on its unit: the unit is set for every texture bound to it.
        glBindSampler(InSlot, m_SamplerID);
    }

    void OpenGLTexture2D::Unbind() const
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindSampler(0, 0);
    }
}
