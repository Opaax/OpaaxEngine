#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "RHI/Framebuffer.h" // OffscreenRenderTarget forwards Bind/size to a concrete IFramebuffer

namespace Opaax
{
    /**
     * @class IRenderTarget
     *
     * Abstraction over "where does the renderer draw to".
     *
     * Runtime : DefaultRenderTarget — draws to the backbuffer (no-op bind).
     * Editor  : OffscreenRenderTarget (below) — draws into an FBO the ViewportPanel owns and samples as a texture (D2, landed M1). 
     *           Generic/engine-side, not editor-named: any future render-to-texture pass reuses it, the editor is just its first caller.
     *
     * The renderer draws through this interface without knowing whether it is rendering to
     * screen or to an offscreen FBO.
     */
    class OPAAX_API IRenderTarget
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        virtual ~IRenderTarget() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        
        /**
         * Called before scene render
         */
        virtual void Bind()     = 0;

        /**
         * Called after scene render
         */
        virtual void Unbind()   = 0;
        
        //------------------------------------------------------------------------------
        // Get

        virtual Uint32 GetWidth()  const noexcept = 0;
        virtual Uint32 GetHeight() const noexcept = 0;
        
        /**
         * The offscreen framebuffer this target draws into, or nullptr for the swapchain backbuffer.
         * A command-buffer backend dispatches on this: null -> present surface, non-null -> render into the framebuffer's image. GL ignores it (binds via Bind()).
         * @return 
         */
        virtual IFramebuffer* GetFramebuffer() const noexcept { return nullptr; }
    };

    /**
     * @class DefaultRenderTarget
     *
     * Draws directly to the GLFW backbuffer.
     * Bind/Unbind are no-ops — the backbuffer is always the default framebuffer.
     * Width/Height come from the GLFW window.
     */
    class OPAAX_API DefaultRenderTarget final : public IRenderTarget
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        DefaultRenderTarget(Uint32 InWidth, Uint32 InHeight)
            : m_Size(InWidth, InHeight)
        {}

        // =============================================================================
        // Functions
        // =============================================================================
        
        /**
         * Called by CoreEngineApp on WindowResizeEvent
         * @param InWidth 
         * @param InHeight 
         */
        void OnResize(Uint32 InWidth, Uint32 InHeight) noexcept
        {
            m_Size.x = InWidth;
            m_Size.y = InHeight;
        }

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin IRenderTarget Interferce
        void Bind()   override {}
        void Unbind() override {}

        Uint32 GetWidth()  const noexcept override { return m_Size.x;  }
        Uint32 GetHeight() const noexcept override { return m_Size.y; }
        //~End IRenderTarget Interferce

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Vector2u32 m_Size;
    };

    /**
     * @class OffscreenRenderTarget
     *
     * Draws into an offscreen framebuffer instead of the backbuffer — the D2 output contract that
     * lets the editor's ViewportPanel sample the world as a texture (and a future render-to-texture
     * pass reuse the same primitive). Bind/Unbind + size forward to the wrapped IFramebuffer;
     * GetFramebuffer() returns it non-null so a command-buffer backend dispatches into the image.
     *
     * NON-OWNING (I5): the FBO is owned by whoever created it (the ViewportPanel in M1). This wrapper
     * only borrows the pointer, so it must not outlive the framebuffer.
     */
    class OPAAX_API OffscreenRenderTarget final : public IRenderTarget
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit OffscreenRenderTarget(IFramebuffer* InFramebuffer) noexcept
            : m_Framebuffer(InFramebuffer)
        {}

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin IRenderTarget Interface
        void Bind()   override { if (m_Framebuffer) { m_Framebuffer->Bind();   } }
        void Unbind() override { if (m_Framebuffer) { m_Framebuffer->Unbind(); } }

        Uint32 GetWidth()  const noexcept override { return m_Framebuffer ? m_Framebuffer->GetWidth()  : 0; }
        Uint32 GetHeight() const noexcept override { return m_Framebuffer ? m_Framebuffer->GetHeight() : 0; }

        // Non-null -> a command-buffer backend renders into this framebuffer's image (never presented).
        IFramebuffer* GetFramebuffer() const noexcept override { return m_Framebuffer; }
        //~End IRenderTarget Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IFramebuffer* m_Framebuffer = nullptr; // non-owning (I5) — the panel owns the FBO
    };

} // namespace Opaax