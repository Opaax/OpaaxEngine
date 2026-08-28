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
         * Queue an outline around EVERY selected entity into the engine's DebugDraw for THIS
         * frame's render. Nothing is retained: the renderer clears the queue every frame, so this
         * re-submits.
         */
        void EnqueueSelectionOutline();

        /**
         * Queue a small box at each entity that draws NOTHING, so an empty entity is visible and
         * clickable — Unreal's editor billboard and Godot's origin grab-area in this engine's
         * terms. Edit worlds only: an editor overlay must not decorate a running game.
         *
         * Sized in world units to cover a fixed number of SCREEN pixels, so it neither vanishes
         * when zoomed out nor swamps the level when zoomed in.
         */
        void EnqueueEntityIcons();

        /**
         * Turn the click or drag MeasureViewportInput banked into a selection.
         *
         * Runs FIRST in OnPreRender — ahead of the resize and the camera gesture — so it reads the
         * exact viewport size and CameraView the clicked frame was RENDERED with. Applying it after
         * either would hit-test against a frame the author never saw.
         */
        void ApplyPendingPick();

        /**
         * How many world units one screen pixel covers, from the active world's view and the current
         * viewport height. ONE conversion, three readers — the entity icon, the gizmo's draw and the
         * gizmo's hit test — which is what makes what-you-see-what-you-click true by construction
         * rather than by keeping constants in step (SEL4).
         *
         * Falls back to 1 (one unit per pixel) when there is no world or no measured height, so a
         * caller never divides by zero or gets a zero-sized handle.
         */
        float WorldPerPixel() const;

        /** The icon's half-size in world units — m_IconHalfPx through WorldPerPixel(). */
        float AnchorHalfExtent() const;

        /**
         * Where the gizmo sits: the centre of the selection's combined bounds, through the same
         * EntityQuery the outline and focus-selected use (SEL1). For ONE entity that is its
         * transform position, since bounds are centred on it — so there is no discrepancy to explain.
         *
         * @return False when there is nothing to draw a gizmo for — no selection, no bounds, or a
         *   PLAY world, which must look like the game rather than like the editor (the rule
         *   EnqueueEntityIcons already states).
         */
        bool TryGetGizmoPivot(Vector2F& OutPivot) const;

        /**
         * Viewport-local pixels -> world, through the ACTIVE WORLD's own view (CAM2's ScreenToWorld).
         * The world's view, not the editor camera's, so picking needs no Edit/Play fork: it asks the
         * world how it was framed and therefore works inside a PIE session too.
         */
        Vector2F ViewportToWorld(const Vector2F& InLocalPx) const;

        /**
         * Read this frame's pan drag and wheel from ImGui and bank them. Call while the panel's
         * window is current and immediately after the image, since it measures the cursor against
         * that item's rect.
         *
         * ImGui is the SOURCE, not a workaround: an Edit world leaves the input route closed, so
         * InputManager never sees a button (IN8). The gate is the IMAGE's hover — never
         * io.WantCaptureMouse (the viewport is itself an ImGui window, L29), and never the WINDOW's,
         * which is true over the title bar and would fight ImGui for the drag.
         *
         * @param bInHovered ImGui::IsItemHovered() taken immediately after the image.
         */
        void MeasureCameraGesture(bool bInHovered);

        /**
         * Read this frame's LEFT button — a click, or a drag that has passed ImGui's own
         * MouseDragThreshold and become a marquee — and bank it for OnPreRender. Same gate and same
         * reasons as MeasureCameraGesture; call it right after the image.
         *
         * The marquee is PAINTED here too, in screen pixels on the foreground draw list, because a
         * selection rectangle is UI rather than world geometry — drawing it in the pass that
         * measures it is also what keeps it free of the one-frame lag everything else here has.
         *
         * @param InOrigin Top-left of the image, in screen pixels — what makes the cursor local.
         */
        void MeasureViewportInput(bool bInHovered, const Vector2F& InOrigin);

        /**
         * Spend what MeasureCameraGesture banked and publish the editor camera as the active world's
         * view. Runs in OnPreRender: after the resize so the pixel sizes are current, and before the
         * engine renders so the result lands in THIS frame.
         */
        void ApplyCameraGesture();

        /**
         * Read this frame's LEFT button against the gizmo's handles and bank what the grab moved.
         * Same gate and same call site as the other two measures; call it right after the image.
         *
         * @return True while the gizmo OWNS the mouse, in which case the caller must not run
         *   MeasureViewportInput — one button, two consumers, and the order is stated here once.
         */
        bool MeasureGizmo(bool bInHovered, const Vector2F& InOrigin);

        /**
         * Spend the banked gizmo motion through EntityOps (SEL6), so a drag is undoable the day ⑤
         * wraps the choke point. Runs in OnPreRender beside ApplyPendingPick, and for the same
         * reason: the motion was measured against the frame that was RENDERED.
         */
        void ApplyGizmoDrag();

        /**
         * Queue the translate handles into DebugDraw for THIS frame — two arrows and a free-move
         * square, every dimension from WorldPerPixel() so the gizmo holds its apparent size at any
         * zoom (Unity's HandleUtility.GetHandleSize in this engine's terms).
         *
         * Runs AFTER ApplyGizmoDrag so the handles are drawn where the entity now is, not where it
         * was when the grab started.
         */
        void EnqueueGizmo();

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

        // Camera gesture, measured in DrawContents and spent in OnPreRender — the same
        // measure-then-apply the resize above uses, and for the same reason: a panel's draw pass
        // reads the world, anything that writes it runs outside the pass.
        Vector2F m_PendingPanPx        = {0.f, 0.f};   // accumulated screen pixels
        Vector2F m_PendingZoomCursorPx = {0.f, 0.f};   // viewport-local, the zoom's anchor
        float    m_PendingZoom         = 0.f;          // wheel notches; + is zoom IN
        bool     m_bPanning            = false;        // the middle button went down over the viewport

        // Selection gesture, measured in DrawContents and spent in OnPreRender. ONE gesture with two
        // outcomes rather than two mechanisms: the press banks a point, and crossing ImGui's drag
        // threshold promotes it to a box. m_bSelecting is what says a press started HERE — a drag
        // that began over another panel must not select.
        enum class EPendingPick : Uint8 { None, Point, Box };

        EPendingPick m_PendingPick    = EPendingPick::None;
        Vector2F     m_PickStartPx    = {0.f, 0.f};   // viewport-local, where the button went down
        Vector2F     m_PickEndPx      = {0.f, 0.f};   // viewport-local, where it came up
        bool         m_bPickAdditive  = false;        // Ctrl was held — add rather than replace
        bool         m_bSelecting     = false;        // the left button is down and started over the image

        // Whether this gesture ever crossed the drag threshold. REMEMBERED rather than queried at
        // release: ImGui::IsMouseDragging needs the button still down, so it is false exactly on the
        // frame the answer is wanted.
        bool         m_bWasDrag       = false;

        // Selection marquee, screen pixels, painted on the foreground list while the drag is live.
        Vector4F m_MarqueeColor     = {1.f, 0.6f, 0.1f, 1.f};
        float    m_MarqueeFillAlpha = 0.12f;

        // The icon for an entity that draws nothing. HALF-size in SCREEN pixels — converted to world
        // units per frame, so it holds its apparent size at any zoom.
        float    m_IconHalfPx    = 9.f;
        Vector4F m_IconColor     = {0.55f, 0.75f, 1.f, 1.f};
        float    m_IconThickness = 2.f;

        // The transform gizmo. Red X / green Y is the convention every reference editor uses, so an
        // author already knows which is which; the highlight is what the hovered or grabbed handle
        // switches to. Sizes are NOT here — they are screen pixels and live with the layout that
        // converts them (GizmoHandles), so the draw and the hit test cannot read different numbers.
        Vector4F m_GizmoAxisXColor    = {0.90f, 0.25f, 0.25f, 1.f};
        Vector4F m_GizmoAxisYColor    = {0.35f, 0.85f, 0.35f, 1.f};
        Vector4F m_GizmoCenterColor   = {0.85f, 0.85f, 0.30f, 1.f};
        Vector4F m_GizmoActiveColor   = {1.f,   1.f,   1.f,   1.f};
        float    m_GizmoThickness     = 2.5f;

        bool   m_bImageLogged    = false;
        bool   m_bOutlineLogged  = false;
        bool   m_bIconsLogged    = false;
        bool   m_bGizmoLogged    = false;
    };
}
