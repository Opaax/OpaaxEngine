#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Camera/EditorCamera.h"      // the preview camera
#include "Editor/EditorUICanvasDocument.h"   // UIWidgetPath
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/UI/EditorRectGeometry.h"    // ERectEdge, TEditorRect
#include "Editor/UI/UIPreviewAspect.h"       // the layout target
#include "Editor/Viewport/ViewportGestures.h" // CameraGesture: middle-drag pan, wheel zoom
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
    // UICanvasPanel — the .opaaxui editor: the widget tree on the left, the canvas on the right.
    //   The preview renders the document's own UICanvas into this panel's framebuffer, so it always
    //   matches what Save writes. The property pane is DrawProperties; edits close through
    //   UICanvasOps::CommitEdit (the step carries the whole tree).
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

        /** Ctrl+D / C / V / Up / Down on this window. Delete is the panel's DeleteCommand. */
        void HandleShortcuts();

        /** The tree: select, drag to reparent, Add, Delete, Duplicate and reorder. */
        void DrawTree();

        /** One node and its subtree. */
        void DrawNode(UIWidget& InWidget, const UIWidgetPath& InPath);

        /** The Add menu: every registered widget type, under the selection. */
        void DrawAddMenu();

        /** The selected widget's fields, or the canvas's when nothing is selected, with undo. */
        void DrawInspector();

        /** The 4x4 anchor grid for InWidget. One undo step; the widget stays in place. */
        void DrawAnchorPresets(UIWidget& InWidget);

        /** The canvas rendered into this panel's framebuffer, and the designer over it. */
        void DrawPreview();

        /**
         * Click selects, drag moves, a grip resizes, arrows nudge (read right after the image).
         * A press on bare canvas clears the selection; a drag is one undo step.
         */
        void MeasurePreviewGesture(bool bInHovered, const Vector2F& InOrigin);

        /** The selection's rect and grips (and the hovered one, faintly) over the image. */
        void DrawPreviewOverlay(const Vector2F& InOrigin);

        /** Moves the selected widget by InDelta canvas units, as one step labelled InLabel. */
        void NudgeSelected(const Vector2F& InDelta, const char* InLabel);

        /** The framebuffer size. */
        Vector2F PreviewPx() const;

        /** The selection's rect in image pixels (where grips are hit-tested and drawn). */
        TEditorRect<float> SelectionRectPx() const;

        // =============================================================================
        // The view — through the preview's camera
        // =============================================================================

        /** The zoomed / panned view the image is rendered through. */
        CameraView PreviewView() const noexcept;

        Vector2F PreviewToCanvas(const Vector2F& InLocalPx) const noexcept;
        Vector2F CanvasToPreview(const Vector2F& InCanvasPoint) const noexcept;
        float    UnitsPerPreviewPixel() const noexcept;

        /** Frames the whole layout target with a margin (F, the Fit button). */
        void FitView();

        /** The canvas at 1:1 (reference pixels = image pixels). */
        void ResetView();

        /** The row above the image: layout aspect, Fit, 1:1 and the zoom percentage. */
        void DrawPreviewToolbar();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        void Startup()     override {}

        /** Submits the document's canvas into this panel's target. */
        void OnPreRender() override;

        void DrawContents() override;

        /** Releases the framebuffer while the device and GL context are alive. */
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

        /** The framebuffer size, and the size measured during the draw (deferred resize). */
        Vector2u32 m_Size        = { 640u, 360u };
        Vector2u32 m_PendingSize = { 0u, 0u };

        /** How much of the panel the tree gets (the author can resize it). */
        float m_TreeWidth = 280.f;

        // =============================================================================
        // The open edit gesture: the tree as it was when the first field became active
        // =============================================================================
        OpaaxString m_GestureBefore;
        bool        m_bGestureOpen   = false;
        bool        m_bWasItemActive = false;

        /** The node being dragged (stored at the drag source). */
        UIWidgetPath m_DragPath;

        // =============================================================================
        // The preview gesture: a press over the image, applied on release
        // =============================================================================
        UIWidgetPath m_HoverPath;                    // what a click would select
        OpaaxString  m_PreviewBefore;                // the tree when the press landed
        Vector2F     m_PreviewAppliedPx = { 0.f, 0.f };   // part of the drag already applied
        bool         m_bPreviewDrag     = false;
        bool         m_bPreviewMoved    = false;

        /** The grip the press landed on (None = a move), and the selection's pixel rect at the press. */
        ERectEdge          m_PreviewEdge = ERectEdge::None;
        TEditorRect<float> m_PressRectPx;

        // =============================================================================
        // The view: measured in the ImGui pass, applied in OnPreRender. Not saved, no undo.
        // =============================================================================
        EditorCamera     m_View;
        CameraGesture    m_ViewGesture;
        EUIPreviewAspect m_Aspect      = EUIPreviewAspect::Free;
        OpaaxString      m_ViewedPath;                // the document the view was set up for
        bool             m_bViewSeeded = false;
        bool             m_bFitPending = false;   // F pressed; framed before the next render
    };
}
