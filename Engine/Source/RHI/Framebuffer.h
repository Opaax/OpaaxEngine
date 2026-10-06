#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // FramebufferSpec
    // =============================================================================
    /**
     * An offscreen render target: one RGBA8 colour attachment and an optional depth/stencil.
     */
    struct FramebufferSpec
    {
        Uint32 Width        = 1;
        Uint32 Height       = 1;
        bool   DepthStencil = true;
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

        // Raw colour attachment handle, for editor display (ImGui::Image) only.
        virtual Uint32 GetColorAttachmentID() const noexcept = 0;
        virtual Uint32 GetWidth()             const noexcept = 0;
        virtual Uint32 GetHeight()            const noexcept = 0;
    };
}
