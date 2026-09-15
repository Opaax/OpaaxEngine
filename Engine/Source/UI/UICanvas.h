#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Renderer/CameraView.h"
#include "UI/UIEvents.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class Renderer2D;
    class UIMask;

    /**
     * ONE quad about to be drawn, with the mask that applies to it (**UI16**).
     *
     * The seam that makes the submit walk TESTABLE: building the list needs no GL context, so
     * "which mask applies to this widget" is asserted headlessly instead of being taken on faith
     * inside a function only a running frame can reach (**TX14**'s one-walk-many-sinks, one level up).
     */
    struct UIDrawItem
    {
        const UIQuad* Quad  = nullptr;   // non-owning; the widget owns it for the frame
        const UIMask* Mask  = nullptr;   // the NEAREST ancestor mask, or null
        Int16         Order = 0;         // tree order, so F5's sort reproduces it
    };

    /** What one Update cost. Zero on an idle frame is the number the design owes. */
    struct UICanvasStats
    {
        Uint32 Layouts  = 0;   // rects resolved
        Uint32 Rebuilds = 0;   // quad lists rebuilt
    };

    // =============================================================================
    // UICanvas — a widget tree and the space it lives in.
    //
    //   CANVAS UNITS ARE REFERENCE PIXELS. The canvas is ReferenceHeight tall, centred on the
    //   origin, and as wide as the target's aspect makes it — its view is a CameraView whose
    //   OrthoSize is half the reference height, so CAM2 does the scaling: a same-aspect resize
    //   changes the projection and nothing in canvas space; only an aspect change moves a rect,
    //   and only the anchored ones.
    //
    //   Owned by whoever hosts it. The host calls SetTargetSize + Update once a frame, Submit
    //   inside its pass, HitTest with a point from ScreenToCanvas.
    // =============================================================================
    class OPAAX_API UICanvas
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

        /** The pixels this canvas is shown in. Same visible rect as last time dirties nothing. */
        void SetTargetSize(Uint32 InWidth, Uint32 InHeight);

        /** The canvas rect the target currently shows — the root's rect. */
        const Bounds2D& GetVisibleBounds() const noexcept { return m_VisibleBounds; }

        /** The view a pass composes this canvas with: origin-centred, half the reference height. */
        CameraView MakeView() const noexcept;

        /** A pixel in the target → canvas units, through the ONE screen→world rule. */
        Vector2F ScreenToCanvas(const Vector2F& InPixel) const noexcept;

        /** The inverse — where a canvas point lands in the target, for an overlay drawn over it. */
        Vector2F CanvasToScreen(const Vector2F& InCanvasPoint) const noexcept;

        /** Canvas units per target pixel — square, since width follows the aspect (CAM2). */
        float UnitsPerPixel() const noexcept;

        // =============================================================================
        // Tree
        // =============================================================================
    public:
        UIWidget&       Root()       noexcept { return *m_Root; }
        const UIWidget& Root() const noexcept { return *m_Root; }

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        /** Resolve and rebuild what is dirty — one walk, once a frame. */
        UICanvasStats Update(const UIBuildContext& InContext = {});

        /**
         * Every visible widget's quads in tree order, each paired with the mask that applies.
         *
         * PURE — no renderer, no GL. `Submit` is this plus the three Draw calls.
         */
        void BuildDrawList(TDynArray<UIDrawItem>& OutItems) const;

        /** Every visible widget's quads, tree order, into the open pass. */
        void Submit(Renderer2D& InRenderer) const;

        /** The top-most hit-testable widget under InCanvasPoint, or null. */
        UIWidget* HitTest(const Vector2F& InCanvasPoint);

        // =============================================================================
        // Input — events bubble from the hit / focused widget (UIEvents.h)
        // =============================================================================
    public:
        /** Move re-hovers (Enter/Leave), Down/Up bubble and capture. @return whether a widget took it. */
        EUIReply RoutePointer(const UIPointerEvent& InEvent);

        /** Bubble from the focused widget; Unhandled when none has focus. */
        EUIReply RouteKey(const UIKeyEvent& InEvent);

        void            SetFocus(UIWidget* InWidget) noexcept;
        UIWidget*       GetFocus()   const noexcept { return m_Focused; }
        const UIWidget* GetHovered() const noexcept { return m_Hovered; }
        const UIWidget* GetPressed() const noexcept { return m_Pressed; }

        /** Leave the hovered widget and drop capture — a switch to GameOnly, or a canvas going dark. */
        void ClearPointer();

        /** A subtree is leaving: forget any hovered / pressed / focused pointer that rests under it. */
        void OnDetached(UIWidget& InSubtreeRoot);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Walk up from InFrom asking each handler; the widget that answered Handled, or null. */
        UIWidget* BubblePointer(UIWidget* InFrom, const UIPointerEvent& InEvent);

        float    m_ReferenceHeight;
        Uint32   m_TargetWidth  = 0;
        Uint32   m_TargetHeight = 0;
        Bounds2D m_VisibleBounds;
        bool     m_bVisibleChanged = true;

        TUniquePtr<UIWidget> m_Root;
        mutable bool         m_bWarnedOrderOverflow = false;

        UIWidget*        m_Hovered      = nullptr;   // non-owning
        UIWidget*        m_Pressed      = nullptr;   // captured between Down and Up
        EUIPointerButton m_PressButton  = EUIPointerButton::None;
        UIWidget*        m_Focused      = nullptr;
    };
}
