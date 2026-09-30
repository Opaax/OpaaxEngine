#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    /**
     * Where the renderer draws: the backbuffer (DefaultRenderTarget) or an offscreen
     * framebuffer (OffscreenRenderTarget, e.g. the editor viewport).
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
         * Called before rendering.
         */
        virtual void Bind()     = 0;

        /**
         * Called after rendering.
         */
        virtual void Unbind()   = 0;
        
        //------------------------------------------------------------------------------
        // Get

        virtual Uint32 GetWidth()  const noexcept = 0;
        virtual Uint32 GetHeight() const noexcept = 0;
        
        /**
         * The offscreen framebuffer, or nullptr for the backbuffer.
         * Command-buffer backends use it; GL binds through Bind().
         */
        virtual IFramebuffer* GetFramebuffer() const noexcept { return nullptr; }
    };

    /**
     * Draws to the window backbuffer. Bind/Unbind do nothing.
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
         * Called on window resize.
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
     * Draws into an offscreen framebuffer (e.g. the editor viewport samples it as a texture).
     * Does not own the framebuffer: do not let it outlive it.
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

        // Non-null: a command-buffer backend renders into this framebuffer (never presented).
        IFramebuffer* GetFramebuffer() const noexcept override { return m_Framebuffer; }
        //~End IRenderTarget Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IFramebuffer* m_Framebuffer = nullptr; // not owned
    };

} // namespace Opaax