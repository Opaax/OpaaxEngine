#pragma once
#include "Renderer/RenderTarget.hpp"

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Editor/EditorState.h"
#include "RHI/Framebuffer.h"

namespace Opaax { class IEditorUIBackend; }

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

        bool Draw(EEditorState State, IEditorUIBackend& InUIBackend);

        //------------------------------------------------------------------------------
        // Get
    public:
        FORCEINLINE bool IsHovered() const noexcept { return m_bHovered; }
        FORCEINLINE bool IsFocused() const noexcept { return m_bFocused; }

        // Screen-absolute position of the content region's top-left (refreshed every Draw).
        // Combined with ImGui::GetMousePos() — which also returns screen-absolute coords
        // when ConfigFlags_ViewportsEnable is on — this gives a docked/undocked-invariant
        // cursor-in-viewport-local position for editor camera input.
        FORCEINLINE Vector2F GetContentScreenPos() const noexcept { return m_ContentScreenPos; }
        FORCEINLINE Vector2F GetContentSize()      const noexcept { return { static_cast<float>(m_Width), static_cast<float>(m_Height) }; }

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin IRenderTarget Interferce
        void Bind()   override;
        void Unbind() override;

        Uint32 GetWidth()  const noexcept override { return m_Width;  }
        Uint32 GetHeight() const noexcept override { return m_Height; }

        // The scene renders into this offscreen target; a command-buffer backend dispatches on it.
        IFramebuffer* GetFramebuffer() const noexcept override { return m_Framebuffer.get(); }
        //~End IRenderTarget Interferce

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Offscreen target — owns the GL handles (no raw GL in this panel anymore).
        UniquePtr<IFramebuffer> m_Framebuffer;

        // Authoritative panel size; the framebuffer is driven from the first Draw's
        // content-region size (no hardcoded default — M7 Step 2 cleanup).
        Uint32 m_Width          = 1;
        Uint32 m_Height         = 1;

        bool   m_bHovered       = false;
        bool   m_bFocused       = false;

        Vector2F m_ContentScreenPos = { 0.f, 0.f };

    public:
        TFunction<void(Uint32, Uint32)> OnResized;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR