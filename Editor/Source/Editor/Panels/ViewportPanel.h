#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"             // TUniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Viewport/ViewportGestures.h"   // pan/zoom and click/marquee, shared with the prefab panel (P8)
#include "Editor/Viewport/ViewportGizmo.h"      // the transform gizmo and the step a drag records, shared too

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
    // ViewportPanel — the dockable "Viewport" panel: the world is rendered into an offscreen FBO
    //   and shown as an ImGui image, so render resolution is decoupled from the window (D2). The
    //   panel OWNS the FBO + its OffscreenRenderTarget wrapper (I5), and SUBMITS it as a render view
    //   every frame in OnPreRender — immediate mode, so there is nothing to unregister.
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
         * Claim a pass in this frame: draw the active world into this panel's FBO, framed the way
         * the world says it is framed, WITH the editor overlays.
         *
         * Re-submitted every frame (F4's idiom) rather than registered once, so nothing has to be
         * cleared when this panel goes away.
         */
        void SubmitView();

        /**
         * Queue an outline around EVERY selected entity into the engine's DebugDraw for THIS
         * frame's render. Nothing is retained: the renderer clears the queue every frame, so this
         * re-submits. The look and the icon-sized fallback live in ViewportOverlays.
         */
        void EnqueueSelectionOutline();

        /**
         * The grid's DRAWN spacing: the translate snap step, raised by whole decades until a cell
         * is at least m_GridMinCellPx across. Zooming out coarsens the grid instead of turning it
         * into a solid field of lines.
         */
        float GridSpacing() const;

        /**
         * What a translate drag actually snaps to.
         *
         * The GRID's spacing while the grid is visible, so a drag lands on a line the author can
         * see — zoomed out, the authored step would put the entity between two of them. The
         * authored step when the grid is hidden, because then there is nothing to match.
         */
        float TranslateSnapStep() const;

        /**
         * Queue the snap grid on the BACKGROUND band, so it sits under everything it measures
         * rather than over it. Spacing IS the gizmo's translate snap step — a grid that does not
         * match what a drag lands on is decoration.
         *
         * Bounded by the visible world rect AND by a decade step-up once a cell would be finer than
         * a few pixels, so zooming out coarsens the grid instead of flooding the batch.
         */
        void EnqueueGrid();

        /**
         * Queue a small box at each entity that draws NOTHING, so an empty entity is visible and
         * clickable. Edit worlds only: an editor overlay must not decorate a running game.
         */
        void EnqueueEntityIcons();

        /**
         * Spend the click or marquee the pick gesture banked, into the global selection.
         *
         * Runs FIRST in OnPreRender — ahead of the resize and the camera gesture — so it reads the
         * exact viewport size and CameraView the clicked frame was RENDERED with. Applying it after
         * either would hit-test against a frame the author never saw.
         */
        void ApplyPendingPick();

        /** The active world's view, or the default frame when there is none. */
        CameraView ActiveView() const;

        /** The image's size in pixels, as a float pair — what every conversion below takes. */
        Vector2F ViewportPx() const;

        /**
         * How many world units one screen pixel covers, from the active world's view and the
         * current viewport height. Falls back to 1 when there is no measured height.
         */
        float WorldPerPixel() const;

        /** The icon's half-size in world units — ViewportOverlays' one value, for the hit test too. */
        float AnchorHalfExtent() const;

        /**
         * Viewport-local pixels -> world, through the ACTIVE WORLD's own view (CAM2's ScreenToWorld).
         * The world's view, not the editor camera's, so picking needs no Edit/Play fork: it asks the
         * world how it was framed and therefore works inside a PIE session too.
         */
        Vector2F ViewportToWorld(const Vector2F& InLocalPx) const;

        /**
         * Place whatever was dropped on the image this frame, AFTER the draw pass (⑦-C).
         *
         * Queued rather than run inline for the Hierarchy's reason (**MP7**): it creates entities,
         * and a panel's draw is a READ of the world. Clearing the request first means a refused
         * drop does not retry itself every frame.
         */
        void RunPendingDrop();

        /**
         * Spend what the camera gesture banked and publish the editor camera as the active world's
         * view. Runs in OnPreRender: after the resize so the pixel sizes are current, and before the
         * engine renders so the result lands in THIS frame.
         */
        void ApplyCameraGesture();

        /**
         * Draw the viewport's tool strip (③b) from EditorExtensionRegistrar::ViewportTools().
         *
         * Called BEFORE the gesture measures and its result handed to them: the strip sits ON the
         * image, and every gesture here gates on the image's hover (**SEL8**), so without
         * subtracting this rect a click on a toolbar button would also start a marquee and a drag
         * off one would pan the camera.
         *
         * @param InOrigin Top-left of the image, in SCREEN pixels.
         * @return Whether the cursor is over the strip.
         */
        bool DrawToolbarOverlay(const Vector2F& InOrigin);

        /**
         * Spend the banked gizmo delta by DISPATCHING the transform command — the route that
         * carries the PIE guard. Then close the drag's step onto the level's stack. Runs in
         * OnPreRender beside ApplyPendingPick, and for the same reason: the motion was measured
         * against the frame that was RENDERED.
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

        // The mouse on the image, measured in DrawContents and spent in OnPreRender — the same
        // measure-then-apply the resize above uses, and for the same reason: a panel's draw pass
        // reads the world, anything that writes it runs outside the pass. Shared types (P8), so
        // the prefab panel's pan, zoom, click, marquee and gizmo are these exact ones.
        CameraGesture m_CameraGesture;
        PickGesture   m_PickGesture;
        GizmoGesture  m_Gizmo;

        // The snap grid. Dim enough to read as paper rather than as content; the two AXES are
        // brighter and coloured like the gizmo's, because a visible origin is worth more than the
        // grid around it. Thicknesses are SCREEN pixels — converted per frame, so the grid stays a
        // hairline at any zoom, unlike the selection outline which is deliberately world-sized.
        Vector4F m_GridColor         = {0.30f, 0.30f, 0.36f, 0.55f};
        Vector4F m_GridAxisXColor    = {0.65f, 0.25f, 0.25f, 0.9f};   // the horizontal line, y == 0
        Vector4F m_GridAxisYColor    = {0.25f, 0.60f, 0.30f, 0.9f};   // the vertical line, x == 0
        float    m_GridThickness     = 1.f;
        float    m_GridAxisThickness = 1.6f;

        // A cell finer than this many pixels steps the spacing up a decade; the cap behind it is a
        // guard, not the mechanism.
        float    m_GridMinCellPx     = 7.f;
        Uint32   m_GridMaxLines      = 600;

        // The tool strip's chrome. Inset from the image corner so it reads as floating ON the
        // viewport rather than welded to it; the SIZE is auto — the strip is exactly as wide as
        // whatever the registry holds, so adding a tool needs no number kept in step here.
        float    m_ToolbarInset    = 8.f;
        float    m_ToolbarRounding = 4.f;
        Vector4F m_ToolbarBg       = {0.10f, 0.10f, 0.12f, 0.85f};

        /** ⑦-C — a prefab dropped on the image this frame, asset-relative. Empty = nothing dropped. */
        OpaaxString  m_PendingDropPrefab;
        Vector2F     m_PendingDropPx  = {0.f, 0.f};   // viewport-local, where it was released

        bool   m_bImageLogged    = false;
        bool   m_bOutlineLogged  = false;
        bool   m_bIconsLogged    = false;
        bool   m_bGridLogged     = false;
    };
}
