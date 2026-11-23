#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Renderer/CameraView.h"
#include "UI/UIBinding.h"
#include "UI/UIEvents.h"
#include "UI/UILayerTable.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class IUIAudio;
    class Renderer2D;

    /**
     * One quad to draw, with the mask that applies to it (built without GL, so it is testable).
     */
    struct UIDrawItem
    {
        const UIQuad*   Quad  = nullptr;   // not owned
        const UIWidget* Mask  = nullptr;   // nearest ancestor that clips its children (a mask, a scroll box), or null
        float           Alpha = 1.f;       // ancestors' opacity times the widget's own, poses included
        Int16           Order = 0;         // tree order

        // The quad as drawn: its widget's and its parents' poses applied. The quad's own values when
        // nothing is posed.
        Vector2F Center   = { 0.f, 0.f };
        Vector2F Size     = { 0.f, 0.f };
        float    Rotation = 0.f;                      // radians, counter-clockwise
        Vector4F Color    = { 1.f, 1.f, 1.f, 1.f };   // the quad's colour times the tints, its alpha times Alpha
        /** The posed mask's box (its bounding box when turned); empty without a mask. */
        Bounds2D MaskRect;
    };

    /** What one Update did. Zero on an idle frame. */
    struct UICanvasStats
    {
        Uint32 Layouts  = 0;   // rects resolved
        Uint32 Rebuilds = 0;   // quad lists rebuilt
    };

    // =============================================================================
    // UICanvas — a widget tree and its space.
    //   Canvas units are reference pixels: the canvas is ReferenceHeight tall, centred on the origin,
    //   and as wide as the target's aspect. Same-aspect resizes change nothing in canvas space.
    //   The host calls SetTargetSize and Update once per frame, Submit inside its pass, and HitTest
    //   with a point from ScreenToCanvas.
    //   The root's trees are kept in layer order (SetLayers, UIWidget::SetLayer): drawn and hit as one
    //   tree, a higher layer on top, with one hovered, pressed and focused widget for all of them.
    // =============================================================================
    class UICanvas
    {
        // =============================================================================
        // CTOR / DTOR
        // =============================================================================
    public:
        explicit UICanvas(float InReferenceHeight = 1080.f);

        UICanvas(const UICanvas&)            = delete;
        UICanvas& operator=(const UICanvas&) = delete;

        // =============================================================================
        // Space
        // =============================================================================
    public:
        float GetReferenceHeight() const noexcept { return m_ReferenceHeight; }
        void  SetReferenceHeight(float InHeight);

        /** The target size in pixels. The same visible rect dirties nothing. */
        void SetTargetSize(Uint32 InWidth, Uint32 InHeight);

        /** The canvas rect the target shows (the root's rect). */
        const Bounds2D& GetVisibleBounds() const noexcept { return m_VisibleBounds; }

        /** The view to draw this canvas with: centred, half the reference height. */
        CameraView MakeView() const noexcept;

        /** Target pixel -> canvas units. */
        Vector2F ScreenToCanvas(const Vector2F& InPixel) const noexcept;

        /** Canvas point -> target pixel. */
        Vector2F CanvasToScreen(const Vector2F& InCanvasPoint) const noexcept;

        /** Canvas units per target pixel. */
        float UnitsPerPixel() const noexcept;

        // =============================================================================
        // Tree
        // =============================================================================
    public:
        UIWidget&       Root()       noexcept { return *m_Root; }
        const UIWidget& Root() const noexcept { return *m_Root; }

        // =============================================================================
        // Layers — where the root's trees draw (UIWidget::SetLayer)
        // =============================================================================
    public:
        /**
         * The layers the root's trees draw in. The trees move at once, equal layers keeping their order;
         * nothing is detached. An empty table (the default) puts every layer level.
         */
        void SetLayers(UILayerTable InLayers);
        const UILayerTable& GetLayers() const noexcept { return m_Layers; }

        /** Where InTree, a tree under the root, draws: its layer's key here. */
        UILayerKey KeyOf(const UIWidget& InTree) const { return m_Layers.KeyOf(InTree.GetLayer()); }

        // =============================================================================
        // Bindings — the named sources this canvas's widgets read
        // =============================================================================
    public:
        UIBindingTable&       Bindings()       noexcept { return m_Bindings; }
        const UIBindingTable& Bindings() const noexcept { return m_Bindings; }

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        /** Reads the bindings, then resolves and rebuilds what is dirty. Once per frame. */
        UICanvasStats Update(const UIBuildContext& InContext = {});

        /**
         * Every visible widget's quads in tree order, with their mask. No renderer needed.
         */
        void BuildDrawList(TDynArray<UIDrawItem>& OutItems) const;

        /** Draws every visible widget's quads into the open pass. */
        void Submit(Renderer2D& InRenderer) const;

        /** The top-most hit-testable widget at InCanvasPoint, or null. */
        UIWidget* HitTest(const Vector2F& InCanvasPoint);

        // =============================================================================
        // Input — events bubble from the hit or focused widget (UIEvents.h)
        // =============================================================================
    public:
        /**
         * Move updates hover (Enter/Leave); Down/Up bubble and capture; Scroll bubbles from the hit widget.
         * @return Whether a widget handled it
         */
        EUIReply RoutePointer(const UIPointerEvent& InEvent);

        /** Bubbles from the focused widget; Unhandled when none has focus. */
        EUIReply RouteKey(const UIKeyEvent& InEvent);

        void            SetFocus(UIWidget* InWidget) noexcept;
        UIWidget*       GetFocus()   const noexcept { return m_Focused; }
        const UIWidget* GetHovered() const noexcept { return m_Hovered; }
        const UIWidget* GetPressed() const noexcept { return m_Pressed; }

        /** Leaves the hovered widget and releases capture. */
        void ClearPointer();

        /**
         * Releases the capture when InWidget holds it: its press ended without a release (a button
         * disabled while held). The Up that follows goes to the widget under the pointer.
         */
        void ReleaseCapture(const UIWidget& InWidget);

        /** A subtree is being removed: forget any pointer state under it. */
        void OnDetached(UIWidget& InSubtreeRoot);

        // =============================================================================
        // Clock — the widgets and modifiers that animate (UIWidget::WakeUp, UIModifier::WakeUp)
        // =============================================================================
    public:
        /**
         * Ticks the awake widgets that are shown, then the awake modifiers, with the frame's own time,
         * and lets go of those that answer false. An awake widget that is not shown leaves the list
         * until Show. Once per frame, after the input is routed and before the canvas is drawn.
         */
        void Tick(float InSeconds);

        /**
         * Everything that ran goes back to rest: the pointer and the focus let go, every transition and
         * tween at rest and asleep, no hide under way. Saved fields are not touched (the editor's preview
         * Stop: the document is as it was).
         */
        void ResetRuntime();

        /** How many modifiers the next Tick runs (none on an idle canvas). */
        Uint64 GetAwakeCount() const noexcept { return m_Awake.size(); }

        /** How many widgets the next Tick may run (none on an idle canvas; a hidden one leaves it then). */
        Uint64 GetAwakeWidgetCount() const noexcept { return m_AwakeWidgets.size(); }

        /**
         * Whether the host ticks this canvas (a game does; the editor's document does only while its
         * preview plays). Widgets joining a ticked canvas are shown: their Show tweens play.
         */
        bool IsTicked() const noexcept { return m_bTicked; }
        void SetTicked(bool bInTicked) noexcept { m_bTicked = bInTicked; }

        /** The asset provider of the last Update (modifiers resolve their presets with it), or null. */
        IUIAssetProvider* GetAssets() const noexcept { return m_Assets; }

        /** What plays the widgets' sounds, set by the host (not owned); null is silence. */
        IUIAudio* GetAudio() const noexcept { return m_Audio; }
        void      SetAudio(IUIAudio* InAudio) noexcept { m_Audio = InAudio; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        friend class UIModifier;
        friend class UIWidget;

        /** Walks up from InFrom; returns the widget that handled it, or null. */
        UIWidget* BubblePointer(UIWidget* InFrom, const UIPointerEvent& InEvent);

        /** RoutePointer's work; RoutePointer then tells the widgets whose state changed. */
        EUIReply RoutePointerTo(const UIPointerEvent& InEvent);

        /** Ticked from the next Tick (once, whatever how many times it is woken). */
        void AddAwake(UIModifier& InModifier);

        /** Not ticked any more. */
        void RemoveAwake(const UIModifier& InModifier);

        /** Ticked from the next Tick while shown (once, whatever how many times it is woken). */
        void AddAwake(UIWidget& InWidget);

        /** Not ticked any more. */
        void RemoveAwake(const UIWidget& InWidget);

        struct InteractionSnapshot
        {
            UIWidget* Hovered = nullptr;
            UIWidget* Pressed = nullptr;
            UIWidget* Focused = nullptr;
        };

        InteractionSnapshot SnapshotInteraction() const noexcept;

        /** Tells every widget on the old and the new pointer and focus chains to work its state out again. */
        void RefreshStates(const InteractionSnapshot& InBefore);

        float    m_ReferenceHeight;
        Uint32   m_TargetWidth  = 0;
        Uint32   m_TargetHeight = 0;
        Bounds2D m_VisibleBounds;
        bool     m_bVisibleChanged = true;

        TStrongObjectPtr<UIWidget> m_Root;
        UIBindingTable       m_Bindings;
        UILayerTable         m_Layers;
        mutable bool         m_bWarnedOrderOverflow = false;

        UIWidget*        m_Hovered      = nullptr;   // not owned
        UIWidget*        m_Pressed      = nullptr;   // captured between Down and Up
        EUIPointerButton m_PressButton  = EUIPointerButton::None;
        UIWidget*        m_Focused      = nullptr;

        bool m_bTicked = true;

        /** From the last Update's context: not owned. */
        IUIAssetProvider* m_Assets = nullptr;
        Uint64            m_AssetsRevision = 0;   // its GetRevision when last seen
        IUIAudio*         m_Audio = nullptr;      // not owned

        /** The modifiers the next Tick runs. Weak: a destroyed one is skipped. */
        TDynArray<TWeakObjectPtr<UIModifier>> m_Awake;

        /** The widgets the next Tick runs when shown. Weak: a destroyed one is skipped. */
        TDynArray<TWeakObjectPtr<UIWidget>> m_AwakeWidgets;
    };
}
