#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // ViewportPanel
    //
    // Renders the active scene into an offscreen framebuffer,
    // then displays the texture inside an ImGui window.
    //
    // This allows the editor layout to coexist with the scene render —
    // the scene does not draw directly to the backbuffer anymore in editor mode.
    //
    // Framebuffer lifecycle:
    //   Startup()     — allocates FBO + color texture + depth renderbuffer.
    //   Shutdown()    — releases GPU resources.
    //   BindFBO()     — called before scene render.
    //   UnbindFBO()   — called after scene render.
    //   Draw()        — displays the color texture in the ImGui window.
    //
    // NOTE: FBO is resized automatically when the ImGui panel is resized.
    // =============================================================================
    class OPAAX_API ViewportPanel
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
        // API
        // =============================================================================
    public:
        bool Startup();
        void Shutdown();

        // Call before scene render
        void BindFBO();

        // Call after scene render
        void UnbindFBO();

        // Draw the ImGui panel
        void Draw();

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        FORCEINLINE Uint32 GetWidth()  const noexcept { return m_Width;  }
        FORCEINLINE Uint32 GetHeight() const noexcept { return m_Height; }
        FORCEINLINE bool   IsHovered() const noexcept { return m_bHovered; }
        FORCEINLINE bool   IsFocused() const noexcept { return m_bFocused; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        void Resize(Uint32 InWidth, Uint32 InHeight);

        Uint32 m_FBO            = 0;
        Uint32 m_ColorTexture   = 0;
        Uint32 m_DepthRBO       = 0;

        Uint32 m_Width          = 1280;
        Uint32 m_Height         = 720;

        bool   m_bHovered       = false;
        bool   m_bFocused       = false;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR