#include "OpenGLTexture2D.h"

#include <glad/glad.h>
#include "Core/Log/OpaaxLog.h"
#include "Core/EngineAPI.h"

// stb_image — implementation defined once here
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace Opaax
{
    OpenGLTexture2D::OpenGLTexture2D(const char* InPath)
    {
        // Flip vertically — stb loads top-left origin, OpenGL expects bottom-left
        stbi_set_flip_vertically_on_load(1);
 
        Int32 lWidth = 0, lHeight = 0, lChannels = 0;
        unsigned char* lData = stbi_load(InPath, &lWidth, &lHeight, &lChannels, 0);
 
        if (!lData)
        {
            OPAAX_CORE_ERROR("OpenGLTexture2D: failed to load '{}'  — {}", InPath, stbi_failure_reason());
            return;
        }
 
        Upload(lData, static_cast<Uint32>(lWidth), static_cast<Uint32>(lHeight), lChannels);
        stbi_image_free(lData);
    }

    OpenGLTexture2D::OpenGLTexture2D(Uint32 InWidth, Uint32 InHeight)
    {
        // White pixel — multiply by tint in shader to get any colour
        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGBA8, static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight));
 
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
 
        const Uint32 lWhite = 0xFFFFFFFF;
        glTextureSubImage2D(m_RendererID, 0, 0, 0,
                            static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight),
                            GL_RGBA, GL_UNSIGNED_BYTE, &lWhite);
 
        m_bLoaded = true;
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::Upload(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        m_Width  = InWidth;
        m_Height = InHeight;
 
        const GLenum lInternalFormat = (InChannels == 4) ? GL_RGBA8  : GL_RGB8;
        const GLenum lDataFormat     = (InChannels == 4) ? GL_RGBA   : GL_RGB;
 
        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, lInternalFormat,
                           static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight));
 
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
 
        glTextureSubImage2D(m_RendererID, 0, 0, 0,
                            static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight),
                            lDataFormat, GL_UNSIGNED_BYTE, InData);
 
        m_bLoaded = true;
    }
    
    void OpenGLTexture2D::Bind(Uint32 InSlot) const { glBindTextureUnit(InSlot, m_RendererID); }
    void OpenGLTexture2D::Unbind() const { glBindTextureUnit(0, 0); }
}
