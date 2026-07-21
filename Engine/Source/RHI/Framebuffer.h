#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // FramebufferSpec
    // =============================================================================
    /**
     * @struct FramebufferSpec
     *
     * Describes an offscreen render target. Minimal on purpose — one RGBA8 color
     * attachment + an optional depth/stencil renderbuffer. Grow only when a pass
     * needs MRT / float formats / sampling the depth.
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
     * @interface IFramebuffer
     *
     * Backend-agnostic offscreen render target. The editor ViewportPanel composes
     * one instead of touching GL directly; future post-process passes render into
     * one. Concrete impl selected by IFramebuffer::Create, defined in the active
     * backend's TU (OpenGLFramebuffer.cpp today).
     */
    class OPAAX_API IFramebuffer
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IFramebuffer() = default;

        // =============================================================================
        // Factory
        // =============================================================================
    public:
        static UniquePtr<IFramebuffer> Create(const FramebufferSpec& InSpec);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // Make this framebuffer the active draw target (also sets the GL viewport to its size).
        virtual void Bind()   = 0;
        // Restore the default (window) framebuffer.
        virtual void Unbind() = 0;

        // Reallocate attachments at a new size. No-op on a zero dimension.
        virtual void Resize(Uint32 InWidth, Uint32 InHeight) = 0;

        //------------------------------------------------------------------------------
        // Get

        // Raw backend handle of the color attachment — for editor display (ImGui::Image) only.
        virtual Uint32 GetColorAttachmentID() const noexcept = 0;
        virtual Uint32 GetWidth()             const noexcept = 0;
        virtual Uint32 GetHeight()            const noexcept = 0;
    };
}
