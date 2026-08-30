#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"             // TUniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Core/String/OpaaxStringID.hpp"
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
         * WHERE the gizmo sits and HOW it is turned — one query, because both answers come from the
         * PRIMARY entity and handles that pointed one way while turning about another would be a lie.
         *
         * The pivot is the selection's combined bounds centre (through the same EntityQuery the
         * outline and focus-selected use, SEL1) or the primary's own position, per EGizmoPivot. The
         * rotation is the primary's when the EFFECTIVE space is Local, and zero otherwise — which is
         * the whole of what makes Local differ from World.
         *
         * @return False when there is nothing to draw a gizmo for — no selection, no bounds, or a
         *   PLAY world, which must look like the game rather than like the editor (the rule
         *   EnqueueEntityIcons already states).
         */
        bool TryGetGizmoPose(Vector2F& OutPivot, float& OutRotationRad) const;

        /**
         * Viewport-local pixels -> world, through the ACTIVE WORLD's own view (CAM2's ScreenToWorld).
         * The world's view, not the editor camera's, so picking needs no Edit/Play fork: it asks the
         * world how it was framed and therefore works inside a PIE session too.
         */
        Vector2F ViewportToWorld(const Vector2F& InLocalPx) const;

        /**
         * Read this frame's pan drag and wheel from ImGui and bank them. Call while the panel's
         * window is current.
         *
         * ImGui is the SOURCE, not a workaround: an Edit world leaves the input route closed, so
         * InputManager never sees a button (IN8). The gate is the IMAGE's hover — never
         * io.WantCaptureMouse (the viewport is itself an ImGui window, L29), and never the WINDOW's,
         * which is true over the title bar and would fight ImGui for the drag.
         *
         * @param bInHovered Hover of the IMAGE, with the toolbar's rect already subtracted.
         * @param InOrigin Top-left of the image in SCREEN pixels — PASSED, not read from
         *   GetItemRect*, which names whatever was submitted last and began naming the toolbar the
         *   moment ③b drew one before this call.
         * @param InSizePx The image's size, for the same reason.
         */
        void MeasureCameraGesture(bool bInHovered, const Vector2F& InOrigin, const Vector2F& InSizePx);

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
         * Draw the transform gizmo and bank whatever the drag produced.
         *
         * ImGuizmo both DRAWS and MANIPULATES in one call, so unlike the other overlays this one
         * lives in the ImGui pass rather than in OnPreRender. What it must not do is write the world
         * from there (MP7) — so the drag's delta is banked and ApplyGizmoDrag spends it.
         *
         * Call it while the panel's window is current and after the image, since it takes the image
         * rect as its viewport.
         *
         * @param InOrigin Top-left of the image, in SCREEN pixels — ImGuizmo::SetRect's frame.
         * @param InSizePx The image's size in screen pixels.
         * @return True while the gizmo owns the mouse, in which case the caller must not run
         *   MeasureViewportInput — one button, two consumers, and the order is stated here once.
         */
        bool MeasureGizmo(const Vector2F& InOrigin, const Vector2F& InSizePx, bool bInSuppress);

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
         * Spend the banked gizmo delta through EntityOps (SEL6), so a drag is undoable the day ⑤
         * wraps the choke point. Runs in OnPreRender beside ApplyPendingPick, and for the same
         * reason: the motion was measured against the frame that was RENDERED.
         */
        void ApplyGizmoDrag();

        /**
         * Keep a live drag inside [InMin, InMax] — the infinite drag. One instrument for both
         * callers (the gizmo and the pan), so the one-shot log below cannot be true for one gesture
         * and untested for the other.
         *
         * @param InGesture Named in that log, because "the wrap never fired" and "it fired and
         *   misbehaved" look identical otherwise, and only one of them is fixable from a log.
         * @return The correction an ABSOLUTE-position reader must accumulate; see ImguiCursor.
         */
        Vector2F WrapDragCursor(const Vector2F& InMin, const Vector2F& InMax, const char* InGesture);

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

        // The tool strip's chrome. Inset from the image corner so it reads as floating ON the
        // viewport rather than welded to it; the SIZE is auto — the strip is exactly as wide as
        // whatever the registry holds, so adding a tool needs no number kept in step here.
        float    m_ToolbarInset    = 8.f;
        float    m_ToolbarRounding = 4.f;
        Vector4F m_ToolbarBg       = {0.10f, 0.10f, 0.12f, 0.85f};

        // Infinite drag: how far the cursor has been TELEPORTED back into the image during the
        // current gizmo drag, accumulated in screen pixels. Added to io.MousePos for the length of
        // the Manipulate call, because ImGuizmo reads the ABSOLUTE position and would otherwise see
        // the wrap as a leap across the viewport. Reset whenever no drag is live.
        //
        // The camera pan needs no equivalent — it reads MouseDelta, which the teleport zeroes.
        Vector2F m_GizmoWrapOffset = {0.f, 0.f};

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

        bool   m_bImageLogged    = false;
        bool   m_bOutlineLogged  = false;
        bool   m_bIconsLogged    = false;
        bool   m_bWrapLogged     = false;

        // One bit per EGizmoMode, not one flag: "does the gizmo write?" is a separate question per
        // mode, and a single one-shot would leave rotate and scale permanently silent after the
        // first translate (L15 — the instrument has to discriminate).
        Uint8  m_GizmoLoggedModes = 0;
    };
}
