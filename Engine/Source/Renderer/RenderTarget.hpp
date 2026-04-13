#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * @class IRenderTarget
     *
     * Abstraction over "where does the renderer draw to".
     *
     * In game mode    : DefaultRenderTarget — draws to the backbuffer (no-op bind).
     * In editor mode  : EditorRenderTarget  — draws to the ViewportPanel FBO.
     *
     * CoreEngineApp holds a raw ptr to the active IRenderTarget.
     *
     * The game layer calls RenderCommand through this interface without knowing whether it is rendering to screen or to an offscreen FBO.
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
            : m_Width(InWidth), m_Height(InHeight)
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
            m_Width  = InWidth;
            m_Height = InHeight;
        }

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin IRenderTarget Interferce
        void Bind()   override {}
        void Unbind() override {}

        Uint32 GetWidth()  const noexcept override { return m_Width;  }
        Uint32 GetHeight() const noexcept override { return m_Height; }
        //~End IRenderTarget Interferce

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_Width;
        Uint32 m_Height;
    };

} // namespace Opaax