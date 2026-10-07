#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/Texture.h"

namespace Opaax
{
    // =============================================================================
    // FramebufferSpec
    // =============================================================================
    /** The format of a framebuffer's colour attachment. */
    enum class ETextureFormat : Uint8
    {
        RGBA8,     // colour, 8 bits per channel
        RGBA16F,   // HDR colour (values above 1)
        R8,        // one 8-bit channel (a mask)
        R16F       // one float channel (distances)
    };

    /**
     * An offscreen render target: one colour attachment and an optional depth/stencil.
     */
    struct FramebufferSpec
    {
        Uint32         Width         = 1;
        Uint32         Height        = 1;
        bool           DepthStencil  = true;
        ETextureFormat ColorFormat   = ETextureFormat::RGBA8;

        /** How the colour is filtered when sampled as a texture (off: nearest texel). */
        bool           bLinearFilter = true;
    };

    // =============================================================================
    // IFramebuffer
    // =============================================================================
    /**
     * Offscreen render target (editor viewport, future post-process passes).
     * Created by IRHIDevice::CreateFramebuffer (editor code: IEngine::CreateFramebuffer).
     */
    class IFramebuffer
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IFramebuffer() = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // Makes it the draw target (also sets the viewport to its size).
        virtual void Bind()   = 0;
        // Restores the default (window) framebuffer.
        virtual void Unbind() = 0;

        // Reallocates the attachments at a new size. Ignores a zero dimension.
        virtual void Resize(Uint32 InWidth, Uint32 InHeight) = 0;

        //------------------------------------------------------------------------------
        // Get

        /** Binds the colour attachment to texture unit InSlot, to be sampled by the next draws. */
        virtual void BindColorTexture(Uint32 InSlot) const = 0;

        // Raw colour attachment handle, for editor display (ImGui::Image) only.
        virtual Uint32 GetColorAttachmentID() const noexcept = 0;
        virtual Uint32 GetWidth()             const noexcept = 0;
        virtual Uint32 GetHeight()            const noexcept = 0;
    };

    // =============================================================================
    // FramebufferTexture — a framebuffer's colour attachment seen as a texture, so a draw can
    //   sample it like any other (a shadow map in a sprite batch). Does not own the framebuffer
    //   and follows its resizes.
    // =============================================================================
    class FramebufferTexture final : public ITexture2D
    {
    public:
        explicit FramebufferTexture(const IFramebuffer& InFramebuffer) noexcept : m_Framebuffer(&InFramebuffer) {}

        //~Begin ITexture2D interface
    public:
        void Bind(Uint32 InSlot = 0) const override { m_Framebuffer->BindColorTexture(InSlot); }

        // Nothing to undo: the next texture bound to the unit replaces it.
        void Unbind() const override {}

        Uint32 GetWidth()      const noexcept override { return m_Framebuffer->GetWidth(); }
        Uint32 GetHeight()     const noexcept override { return m_Framebuffer->GetHeight(); }
        Uint32 GetRendererID() const noexcept override { return m_Framebuffer->GetColorAttachmentID(); }
        bool   IsLoaded()      const noexcept override { return m_Framebuffer->GetColorAttachmentID() != 0; }
        //~End ITexture2D interface

    private:
        const IFramebuffer* m_Framebuffer = nullptr;
    };
}
