#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/Texture.h"

namespace Opaax
{
    /**
     * OpenGL ITexture2D. Uploads decoded pixels (R8, RGB or RGBA). R8 is swizzled into alpha,
     * so the RGBA sprite shader reads (1,1,1,coverage). Does not read files (see TextureResource).
     */
    class OPAAX_API OpenGLTexture2D final : public ITexture2D
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /**
         * Solid white texture (coloured quads multiply it by the tint).
         */
        OpenGLTexture2D(Uint32 InWidth, Uint32 InHeight);

        /**
         * Uploads raw pixels. Channels: 4 = RGBA8, 3 = RGB8, 1 = R8 (swizzled into alpha).
         * InData is copied.
         */
        OpenGLTexture2D(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

        ~OpenGLTexture2D();

        // =============================================================================
        // Copy - delete
        // =============================================================================
        OpenGLTexture2D(const OpenGLTexture2D&)            = delete;
        OpenGLTexture2D& operator=(const OpenGLTexture2D&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        OpenGLTexture2D(OpenGLTexture2D&&)                 = default;
        OpenGLTexture2D& operator=(OpenGLTexture2D&&)      = default;

        // =============================================================================
        // Function
        // =============================================================================
    private:
        void Upload(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ITexture2D interface
    public:
        void Bind(Uint32 InSlot = 0) const override;
        void Unbind()                const override;

        //------------------------------------------------------------------------------
        //Get

        FORCEINLINE Uint32 GetWidth()      const noexcept override { return m_Width;      }
        FORCEINLINE Uint32 GetHeight()     const noexcept override { return m_Height;     }
        FORCEINLINE Uint32 GetRendererID() const noexcept override { return m_RendererID; }
        FORCEINLINE bool   IsLoaded()      const noexcept override { return m_bLoaded;    }
        //~End ITexture2D interface

        // =============================================================================
        // Operators
        // =============================================================================
    public:
        bool operator==(const OpenGLTexture2D& Other) const noexcept
        {
            return m_RendererID == Other.m_RendererID;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_RendererID = 0;
        Uint32 m_Width      = 0;
        Uint32 m_Height     = 0;
        bool   m_bLoaded    = false;
    };
 
} // namespace Opaax
