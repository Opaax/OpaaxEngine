#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"             // TUniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Viewport/ViewportGestures.h"   // pan/zoom, click/marquee (shared with the prefab panel)
#include "Editor/Viewport/ViewportGizmo.h"      // transform gizmo (shared too)

namespace Opaax
{
    class IFramebuffer;
    class OffscreenRenderTarget;
    struct CameraView;

    OPAAX_LOG_CATEGORY(ViewportPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ViewportPanel — the "Viewport": the world rendered into an offscreen FBO and shown as an ImGui
    //   image (render resolution is independent of the window). Owns the FBO and submits it as a
    //   render view every frame in OnPreRender.
    //   Resizing is deferred by one frame (no reallocation between render and sample):
    //     DrawContents() (end of frame N)     measures the content region -> pending size.
    //     OnPreRender()  (start of frame N+1) applies it before the world renders.
    //   Hover/focus is measured here and pushed into InputRoute.
    // =============================================================================
    class ViewportPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Viewport);
        
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
         * The FBO as an ImGui image (upright). A null handle draws a Dummy of the same size.
         */
        EditorImage GetViewportImage() const;

        /**
         * Resizes the FBO to the size measured last frame, if it changed.
         */
        void ApplyPendingResize();

        /**
         * Submits this frame's view: the active world into this panel's FBO, with editor overlays.
         */
        void SubmitView();

        /**
         * Queues an outline around every selected entity for this frame (see ViewportOverlays).
         */
        void EnqueueSelectionOutline();

        /**
         * The grid's drawn spacing: the translate snap step, raised by decades until a cell is at least
         * m_GridMinCellPx wide.
         */
        float GridSpacing() const;

        /**
         * What a translate drag snaps to: the grid spacing while the grid is visible, else the authored step.
         */
        float TranslateSnapStep() const;

        /**
         * Queues the snap grid on the Background layer. Only visible lines, coarser when zoomed out.
         */
        void EnqueueGrid();

        /**
         * Queues a small box at each entity that draws nothing, so it is visible and clickable. Edit only.
         */
        void EnqueueEntityIcons();

        /**
         * Applies the click or marquee the pick gesture stored. First in OnPreRender, so it uses the
         * size and view the clicked frame was rendered with.
         */
        void ApplyPendingPick();

        /** The active world's view, or the default view. */
        CameraView ActiveView() const;

        /** The image size in pixels. */
        Vector2F ViewportPx() const;

        /**
         * World units per screen pixel (1 when no height is measured).
         */
        float WorldPerPixel() const;

        /** The icon's half-size in world units (also used for hit tests). */
        float AnchorHalfExtent() const;

        /**
         * Viewport pixels -> world, through the active world's view (works in Edit and Play).
         */
        Vector2F ViewportToWorld(const Vector2F& InLocalPx) const;

        /**
         * Places what was dropped on the image this frame, after the draw pass (it creates entities).
         * Cleared first, so a refused drop does not retry.
         */
        void RunPendingDrop();

        /**
         * Applies the camera gesture and publishes the editor camera as the active world's view.
         * After the resize, before the engine renders.
         */
        void ApplyCameraGesture();

        /**
         * Draws the viewport tool strip (from ViewportTools()), before the gestures: its rect is removed
         * from the image hover so a button click does not start a marquee.
         * @param InOrigin Top-left of the image, in screen pixels
         * @return Whether the cursor is over the strip
         */
        bool DrawToolbarOverlay(const Vector2F& InOrigin);

        /**
         * Applies the stored gizmo delta through the transform command (Play guard), then closes the
         * drag's undo step. In OnPreRender, like ApplyPendingPick.
         */
        void ApplyGizmoDrag();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        void            Startup()               override;
        void            OnPreRender()           override;
        void            DrawContents()          override;
        void            Shutdown()              override;

        /** No padding: the image fills the window. */
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

        // Mouse gestures on the image: measured in DrawContents, applied in OnPreRender. Shared with the
        // prefab panel.
        CameraGesture m_CameraGesture;
        PickGesture   m_PickGesture;
        GizmoGesture  m_Gizmo;

        // The snap grid: dim, with coloured axes. Thicknesses are in screen pixels.
        Vector4F m_GridColor         = {0.30f, 0.30f, 0.36f, 0.55f};
        Vector4F m_GridAxisXColor    = {0.65f, 0.25f, 0.25f, 0.9f};   // the horizontal line, y == 0
        Vector4F m_GridAxisYColor    = {0.25f, 0.60f, 0.30f, 0.9f};   // the vertical line, x == 0
        float    m_GridThickness     = 1.f;
        float    m_GridAxisThickness = 1.6f;

        // A cell finer than this many pixels steps the spacing up a decade.
        float    m_GridMinCellPx     = 7.f;
        Uint32   m_GridMaxLines      = 600;

        // The tool strip: inset from the image corner; its size is automatic.
        float    m_ToolbarInset    = 8.f;
        float    m_ToolbarRounding = 4.f;
        Vector4F m_ToolbarBg       = {0.10f, 0.10f, 0.12f, 0.85f};

        /** A prefab dropped on the image this frame (asset-relative). Empty = none. */
        OpaaxString  m_PendingDropPrefab;
        Vector2F     m_PendingDropPx  = {0.f, 0.f};   // viewport-local release point

    };
}
