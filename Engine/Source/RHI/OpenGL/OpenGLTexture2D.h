#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // OpenGLTexture2D
    //
    // Loads an image via stb_image and uploads it to the GPU.
    // Supports RGBA and RGB source images — always stored as RGBA on GPU.
    // =============================================================================

    /**
     * @class OpenGLTexture2D
     *
     * Loads an image via stb_image and uploads it to the GPU.
     * Supports RGBA and RGB source images — always stored as RGBA on GPU.
     */
    class OPAAX_API OpenGLTexture2D
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /**
         * Load from file path (stb_image)
         * @param InPath 
         */
        explicit OpenGLTexture2D(const char* InPath);
        
        /**
         * Create a 1x1 solid colour texture — useful for coloured quads without
         * needing a real texture (white pixel * tint colour in the shader)
         * @param InWidth 
         * @param InHeight 
         */
        OpenGLTexture2D(Uint32 InWidth, Uint32 InHeight);
 
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
        
    public:
        void Bind(Uint32 InSlot = 0) const;
        void Unbind()                const;

        //------------------------------------------------------------------------------
        //Get - Set
        
        FORCEINLINE Uint32 GetWidth()      const noexcept { return m_Width;      }
        FORCEINLINE Uint32 GetHeight()     const noexcept { return m_Height;     }
        FORCEINLINE Uint32 GetRendererID() const noexcept { return m_RendererID; }
        FORCEINLINE bool   IsLoaded()      const noexcept { return m_bLoaded;    }

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
