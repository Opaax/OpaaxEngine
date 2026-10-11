#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Renderer/CameraView.h"
#include "UI/UIBinding.h"
#include "UI/UIEvents.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class Renderer2D;
    class UIMask;

    /**
     * One quad to draw, with the mask that applies to it (built without GL, so it is testable).
     */
    struct UIDrawItem
    {
        const UIQuad* Quad  = nullptr;   // not owned
        const UIMask* Mask  = nullptr;   // nearest ancestor mask, or null
        float         Alpha = 1.f;       // ancestors' opacity times the widget's own
        Int16         Order = 0;         // tree order
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
        /** Move updates hover (Enter/Leave); Down/Up bubble and capture. @return Whether a widget handled it. */
        EUIReply RoutePointer(const UIPointerEvent& InEvent);

        /** Bubbles from the focused widget; Unhandled when none has focus. */
        EUIReply RouteKey(const UIKeyEvent& InEvent);

        void            SetFocus(UIWidget* InWidget) noexcept;
        UIWidget*       GetFocus()   const noexcept { return m_Focused; }
        const UIWidget* GetHovered() const noexcept { return m_Hovered; }
        const UIWidget* GetPressed() const noexcept { return m_Pressed; }

        /** Leaves the hovered widget and releases capture. */
        void ClearPointer();

        /** A subtree is being removed: forget any pointer state under it. */
        void OnDetached(UIWidget& InSubtreeRoot);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Walks up from InFrom; returns the widget that handled it, or null. */
        UIWidget* BubblePointer(UIWidget* InFrom, const UIPointerEvent& InEvent);

        float    m_ReferenceHeight;
        Uint32   m_TargetWidth  = 0;
        Uint32   m_TargetHeight = 0;
        Bounds2D m_VisibleBounds;
        bool     m_bVisibleChanged = true;

        TUniquePtr<UIWidget> m_Root;
        UIBindingTable       m_Bindings;
        mutable bool         m_bWarnedOrderOverflow = false;

        UIWidget*        m_Hovered      = nullptr;   // not owned
        UIWidget*        m_Pressed      = nullptr;   // captured between Down and Up
        EUIPointerButton m_PressButton  = EUIPointerButton::None;
        UIWidget*        m_Focused      = nullptr;
    };
}
