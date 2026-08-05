#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"             // TUniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/UI/IEditorUIBackend.h"

namespace Opaax
{
    class IFramebuffer;
    class OffscreenRenderTarget;
    
    OPAAX_LOG_CATEGORY(ViewportPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ViewportPanel — the dockable "Viewport" panel: the world is rendered into an offscreen FBO
    //   and shown as an ImGui image, so render resolution is decoupled from the window (D2). The
    //   panel OWNS the FBO + its OffscreenRenderTarget wrapper (I5), and registers the target as the
    //   engine's primary render target for its whole lifetime (Startup sets it, Shutdown clears it).
    //
    //   Resize is deferred by one frame to avoid reallocating the FBO between the world render and
    //   the sample within a single frame:
    //     Draw()        (end of frame N)   MEASURES the panel's content region -> caches a pending size.
    //     OnPreRender() (start of frame N+1) APPLIES the pending size BEFORE the world renders.
    //   The one-frame lag is normal for ImGui render-to-texture (not a bug).
    // =============================================================================
    class ViewportPanel final : public IEditorPanel
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit ViewportPanel(EditorContext& InContext);
        ~ViewportPanel() override;
        
        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        ViewportPanel(const ViewportPanel&)            = delete;
        ViewportPanel& operator=(const ViewportPanel&) = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * A null handle -> reserve space with a Dummy so the layout is unchanged (drawing a null texture is a backend validation error).
         * @return Sample the FBO the world rendered into this frame. The backend yields the ImGui handle + the UVs that present it upright (GL FBOs are bottom-up).
         */
        EditorViewportImage GetViewportImage() const;

        /**
         * Resize the FBO to the size Draw() measured last frame, if it changed. Early-outs on the
         * steady-state (nothing pending / same size), which is why it is its own method — see
         * OnPreRender.
         */
        void ApplyPendingResize();

        /**
         * Queue the selected entity's outline into the engine's DebugDraw for THIS frame's render.
         * Nothing is retained: the renderer clears the queue every frame, so this re-submits.
         */
        void EnqueueSelectionOutline();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        void            Startup()               override;
        void            OnPreRender()           override;
        void            Draw()                  override;
        void            Shutdown()              override;
        OpaaxStringID   GetPanelID()    const   override { return m_PanelID; }
        //~End IEditorPanel interface

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /**
         * Whether the pointer / the keyboard are on the viewport — D5 step 2's gate (M-Input).
         *
         * Measured in Draw(), which runs at the END of a frame, so a reader during the next
         * frame's input poll is one frame behind. Normal for ImGui state and harmless here: the
         * gate changes when the user moves between panels, not within a frame.
         */
        bool IsHovered() const noexcept { return m_bHovered; }
        bool IsFocused() const noexcept { return m_bFocused; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;
        
        const OpaaxStringID m_PanelID{ OPAAX_ID("Viewport") };
        const OpaaxString   m_Title = m_PanelID.ToString();

        TUniquePtr<IFramebuffer>          m_Framebuffer;
        TUniquePtr<OffscreenRenderTarget> m_RenderTarget;
        
        Vector2F   m_viewportSizeDefault  = {960.f, 600.f};
        Vector2u32 m_viewportSize         = {1,1};
        Vector2u32 m_viewportPendingSize  = {1,1};

        // Selection outline. Orange because no Sandbox quad is; the padding pushes the border off the
        // quad's own edge so it reads as an outline rather than a repaint of its rim. Both are in
        // WORLD units — 1 unit = 1px only at the FBO's native size, so the border thins visually when
        // the panel is scaled up. Acceptable for a debug overlay; screen-space width would need the
        // view scale here, which the panel does not own.
        // Tuned against the Sandbox's 120x120 quads: the border sits ~1.5 units clear of the quad
        // edge and is 3 units thick — unmistakable without swamping a small entity.
        Vector4F m_OutlineColor     = {1.f, 0.6f, 0.1f, 1.f};
        Vector2F m_OutlinePadding   = {6.f, 6.f};
        float    m_OutlineThickness = 3.f;

        bool   m_bImageLogged    = false;
        bool   m_bOutlineLogged  = false;

        // D5 step 2, refreshed every Draw. False until the first one — before the panel has been
        // drawn there is nothing for the pointer to be over.
        bool   m_bHovered        = false;
        bool   m_bFocused        = false;
    };
}
