#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    // =============================================================================
    // OpenGLTextureTarget — draws into an existing texture (a paintable one): a framebuffer whose
    //   colour attachment is that texture's full-size level. It owns only the framebuffer object;
    //   the texture must outlive it. No depth, no multisampling, and its size is the texture's.
    // =============================================================================
    class OpenGLTextureTarget final : public IFramebuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit OpenGLTextureTarget(const ITexture2D& InTexture);
        ~OpenGLTextureTarget() override;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        OpenGLTextureTarget(const OpenGLTextureTarget&)            = delete;
        OpenGLTextureTarget& operator=(const OpenGLTextureTarget&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IFramebuffer interface
    public:
        void Bind()   override;
        void Unbind() override;

        /** Refused (with a warning): the size is the texture's. */
        void Resize(Uint32 InWidth, Uint32 InHeight) override;

        void BindColorTexture(Uint32 InSlot) const override;

        Uint32 GetColorAttachmentID() const noexcept override;
        Uint32 GetWidth()             const noexcept override;
        Uint32 GetHeight()            const noexcept override;
        bool   ReadPixels(TDynArray<Uint8>& OutRGBA) const override;
        //~End IFramebuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        const ITexture2D* m_Texture = nullptr;   // not owned
        Uint32            m_FBO     = 0;
    };
}
