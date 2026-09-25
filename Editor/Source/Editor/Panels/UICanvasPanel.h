#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Camera/EditorCamera.h"      // the preview's view — the world viewport's camera, reused (U13)
#include "Editor/EditorUICanvasDocument.h"   // UIWidgetPath
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/UI/EditorRectGeometry.h"    // the grips: ERectEdge, TEditorRect
#include "Editor/UI/UIPreviewAspect.h"       // the layout target, now that the view is not the whole canvas
#include "Editor/Viewport/ViewportGestures.h" // CameraGesture — middle-drag pan, wheel zoom at the cursor
#include "Renderer/CameraView.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UICanvasPanel);

    class IFramebuffer;
    class OffscreenRenderTarget;
    class UIWidget;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // UICanvasPanel — the `.opaaxui` EDITOR: the widget tree on the left, the canvas itself on the
    //   right (**UI14**).
    //
    //   THE PREVIEW IS THE DOCUMENT, not a picture of it: the panel submits the document's own
    //   UICanvas into its own framebuffer, so what is on screen and what a Save writes cannot
    //   disagree. That is why the canvas submission learned to name a target — the game's canvases
    //   must not pour in here, nor this one into the world.
    //
    //   A widget is CReflected, so the property pane is `DrawProperties` and nothing else: a field
    //   added to a widget type appears here with no change to this panel. What the drawer cannot
    //   know is when a GESTURE ended, which is why the edit closes through UICanvasOps::CommitEdit —
    //   FontFamilyPanel's shape, with the whole tree as the before-image (**UI15**).
    // =============================================================================
    class UICanvasPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(UI Canvas);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit UICanvasPanel(EditorContext& InContext);
        ~UICanvasPanel() override;

        UICanvasPanel(const UICanvasPanel&)            = delete;
        UICanvasPanel& operator=(const UICanvasPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker, Save, and the widget count. */
        void DrawHeader();

        /** Ctrl+D / C / V / Up / Down on this window — the selection's verbs; Delete is the panel's DeleteCommand. */
        void HandleShortcuts();

        /** The tree: select, drag to reparent, Add, Delete, Duplicate and reorder. */
        void DrawTree();

        /** One node and its subtree, recursively. */
        void DrawNode(UIWidget& InWidget, const UIWidgetPath& InPath);

        /** The Add menu — every registered widget type, under the selection. */
        void DrawAddMenu();

        /** The selected widget's fields — or the canvas's own when nothing is — bracketed for undo. */
        void DrawInspector();

        /** Unity's 4x4 anchor grid for InWidget; a click is one step and keeps the widget where it is. */
        void DrawAnchorPresets(UIWidget& InWidget);

        /** The canvas rendered into this panel's own framebuffer, and the designer over it. */
        void DrawPreview();

        /**
         * Click selects, drag moves, a grip resizes, arrows nudge — read right after the image, while
         * its window is current. A press on bare canvas clears the selection; a drag is ONE undo step.
         */
        void MeasurePreviewGesture(bool bInHovered, const Vector2F& InOrigin);

        /** The selection's rect with its grips (and, faintly, the hovered one) over the image — how an invisible container is seen. */
        void DrawPreviewOverlay(const Vector2F& InOrigin);

        /** Move the selected widget by InDelta canvas units as one step labelled InLabel. */
        void NudgeSelected(const Vector2F& InDelta, const char* InLabel);

        /** The framebuffer's size, as floats. */
        Vector2F PreviewPx() const;

        /** The selection's resolved rect in image pixels — what the grips are hit-tested and drawn on. */
        TEditorRect<float> SelectionRectPx() const;

        // =============================================================================
        // The view — CAM2's one screen<->world rule, through the preview's camera, not the canvas's
        // =============================================================================

        /** The zoomed / panned view the image is rendered and read through (U13). */
        CameraView PreviewView() const noexcept;

        Vector2F PreviewToCanvas(const Vector2F& InLocalPx) const noexcept;
        Vector2F CanvasToPreview(const Vector2F& InCanvasPoint) const noexcept;
        float    UnitsPerPreviewPixel() const noexcept;

        /** Frame the whole layout target with a margin — `F`, the toolbar's Fit. */
        void FitView();

        /** The canvas at 1:1 — reference pixels are image pixels, today's picture before U13. */
        void ResetView();

        /** The row above the image: the layout aspect, Fit, 1:1, and the zoom as a percentage. */
        void DrawPreviewToolbar();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        void Startup()     override {}

        /** Submits the document's canvas into this panel's target — the frame's UI work (UI14). */
        void OnPreRender() override;

        void DrawContents() override;

        /** Release the framebuffer while the device and its GL context are both alive (LC3). */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 900.f, 560.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        TUniquePtr<IFramebuffer>          m_Framebuffer;
        TUniquePtr<OffscreenRenderTarget> m_RenderTarget;

        /** The framebuffer's size, and the size measured during the draw (deferred resize). */
        Vector2u32 m_Size        = { 640u, 360u };
        Vector2u32 m_PendingSize = { 0u, 0u };

        /** How much of the panel the tree gets. ImGui's ResizeX lets the author move it. */
        float m_TreeWidth = 280.f;

        // =============================================================================
        // The open edit gesture — the TREE as it was when the first field went active
        // =============================================================================
        OpaaxString m_GestureBefore;
        bool        m_bGestureOpen   = false;
        bool        m_bWasItemActive = false;

        /** The node a drag is carrying, banked at the drag source (the Hierarchy's rule). */
        UIWidgetPath m_DragPath;

        // =============================================================================
        // The preview gesture — a press over the image, spent on release
        // =============================================================================
        UIWidgetPath m_HoverPath;                    // what a click would take
        OpaaxString  m_PreviewBefore;                // the tree when the press landed
        Vector2F     m_PreviewAppliedPx = { 0.f, 0.f };   // of the drag's total, already applied
        bool         m_bPreviewDrag     = false;
        bool         m_bPreviewMoved    = false;

        /** The grip the press landed on (None = a move), and the selection's pixel rect at that press. */
        ERectEdge          m_PreviewEdge = ERectEdge::None;
        TEditorRect<float> m_PressRectPx;

        // =============================================================================
        // The view (U13) — measured in the ImGui pass, spent in OnPreRender (SEL3). View state,
        // not document state: no undo step, not saved.
        // =============================================================================
        EditorCamera     m_View;
        CameraGesture    m_ViewGesture;
        EUIPreviewAspect m_Aspect      = EUIPreviewAspect::Free;
        OpaaxString      m_ViewedPath;                // the document the view was seeded for
        bool             m_bViewSeeded = false;
        bool             m_bFitPending = false;   // F pressed during the draw; framed before the next render
    };
}
