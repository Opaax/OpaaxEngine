#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(OpenGLFramebuffer);
    
    /**
     * OpenGL IFramebuffer: one colour texture (RGBA8, RGBA16F, R8 or R16F) and an optional
     * GL_DEPTH24_STENCIL8 renderbuffer.
     */
    class OpenGLFramebuffer final : public IFramebuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /**
         * @param InAllocations Raised each time the framebuffer gets new storage (at creation and
         *                      at each change of size). Outlives the framebuffer; may be null.
         */
        explicit OpenGLFramebuffer(const FramebufferSpec& InSpec, Uint64* InAllocations = nullptr);
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
        void BindColorTexture(Uint32 InSlot) const override;

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
        bool   m_DepthStencil = false;

        ETextureFormat m_Format        = ETextureFormat::RGBA8;
        bool           m_bLinearFilter = true;

        Uint64* m_Allocations = nullptr;   // the device's count
    };
}
