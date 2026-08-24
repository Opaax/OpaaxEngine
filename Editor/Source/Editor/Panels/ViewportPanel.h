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
    //     DrawContents() (end of frame N)   MEASURES the panel's content region -> caches a pending size.
    //     OnPreRender()  (start of frame N+1) APPLIES the pending size BEFORE the world renders.
    //   The one-frame lag is normal for ImGui render-to-texture (not a bug).
    //
    //   Viewport hover/focus (D5 step 2) is measured here and PUSHED into InputRoute, so nothing
    //   needs a typed pointer to this panel to route input.
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
        EditorImage GetViewportImage() const;

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
        void            DrawContents()          override;
        void            Shutdown()              override;

        /** Zero padding: the world image fills the window edge to edge. */
        PanelWindowStyle GetWindowStyle() const override { return { m_viewportSizeDefault, true }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

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
    };
}
