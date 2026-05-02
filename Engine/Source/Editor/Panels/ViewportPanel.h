#pragma once
#include "Renderer/RenderTarget.hpp"

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Editor/EditorState.h"

namespace Opaax::Editor
{
    /**
     * @class ViewportPanel
     *
     * Renders the active scene into an offscreen FBO, then displays the texture inside an ImGui window.
     *
     * This allows the editor layout to coexist with the scene render — the scene does not draw directly to the backbuffer anymore in editor mode.
     *
     * Framebuffer lifecycle:
     *  Startup()     — allocates FBO + color texture + depth renderbuffer.
     *  Shutdown()    — releases GPU resources.
     *  BindFBO()     — called before scene render.
     *  UnbindFBO()   — called after scene render.
     *  Draw()        — displays the color texture in the ImGui window.
     */
    class OPAAX_API ViewportPanel final : public IRenderTarget
    { 
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        ViewportPanel()  = default;
        ~ViewportPanel() = default;

        ViewportPanel(const ViewportPanel&)            = delete;
        ViewportPanel& operator=(const ViewportPanel&) = delete;

        // =============================================================================
        // FUNCTION
        // =============================================================================
    private:
        void Resize(Uint32 InWidth, Uint32 InHeight);
    public:
        bool Startup();
        void Shutdown();

        bool Draw(EEditorState State);

        //------------------------------------------------------------------------------
        // Get
    public:
        FORCEINLINE bool IsHovered() const noexcept { return m_bHovered; }
        FORCEINLINE bool IsFocused() const noexcept { return m_bFocused; }

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin IRenderTarget Interferce
        void Bind()   override;
        void Unbind() override;

        Uint32 GetWidth()  const noexcept override { return m_Width;  }
        Uint32 GetHeight() const noexcept override { return m_Height; }
        //~End IRenderTarget Interferce

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_FBO            = 0;
        Uint32 m_ColorTexture   = 0;
        Uint32 m_DepthRBO       = 0;
                                
        Uint32 m_Width          = 1280;
        Uint32 m_Height         = 720;
                                
        bool   m_bHovered       = false;
        bool   m_bFocused       = false;

    public:
        TFunction<void(Uint32, Uint32)> OnResized;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR