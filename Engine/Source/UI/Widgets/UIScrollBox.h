#pragma once

#include "Core/OpaaxTypes.h"
#include "UI/UIMargin.h"
#include "UI/UIStackLayout.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    // =============================================================================
    // UIScrollBox — a list that scrolls, like Unreal's UScrollBox: lays its children out one after
    //   another like a UIStack, shows them only inside its rect like a UIMask, and moves them by the
    //   wheel (ScrollStep a notch) and by dragging. Hit-testable, so the wheel and a drag work on its
    //   empty parts; a press a child takes (a button) does not drag. When all of its content shows, it
    //   lets the wheel and presses through. Draws nothing itself.
    // =============================================================================
    class UIScrollBox final : public UIWidget
    {
        OPAAX_OBJECT(UIScrollBox, UIWidget)

        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        EUIAxis  Axis       = EUIAxis::Vertical;
        float    Spacing    = 0.f;     // between children, canvas units
        UIMargin Padding;              // inside my rect, canvas units
        EUIAlign ChildAlign = EUIAlign::Stretch;

        /** How far one notch of the wheel moves the content, canvas units. */
        float ScrollStep = 60.f;

        /** Dragging with the primary button moves the content. */
        bool bDragToScroll = true;

        OPAAX_PROPERTIES(UIScrollBox,
                         OPAAX_PROP(Axis),
                         OPAAX_PROP(Spacing).SetDragStep(1.f),
                         OPAAX_PROP(Padding).SetDragStep(1.f),
                         OPAAX_PROP(ChildAlign),
                         OPAAX_PROP(ScrollStep).SetRange(0.f, 2000.f).SetDragStep(1.f)
                             .SetTooltip("Canvas units one notch of the wheel moves the content."),
                         OPAAX_PROP(bDragToScroll).SetTooltip("Dragging with the left button moves the content."))

        void SetAxis(EUIAxis InAxis);
        void SetSpacing(float InSpacing);
        void SetPadding(const UIMargin& InPadding);
        void SetChildAlign(EUIAlign InAlign);

        // =============================================================================
        // Scrolling
        // =============================================================================
    public:
        /** How far the content is scrolled along the axis: 0 shows its start, GetMaxScroll() its end. */
        float GetScroll() const noexcept;

        /** The part of the content that does not fit, along the axis: 0 when all of it shows. */
        float GetMaxScroll() const noexcept;

        /** Scrolls to InScroll, kept between 0 and GetMaxScroll(). */
        void SetScroll(float InScroll);

        void ScrollBy(const float InDelta) { SetScroll(GetScroll() + InDelta); }

        /** True from a press that began a drag until its release. */
        bool IsDragging() const noexcept { return m_bDragging; }

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        UIScrollBox();

        EUIReply OnPointerEvent(const UIPointerEvent& InEvent) override;

    protected:
        void ArrangeChildren(TDynArray<Bounds2D>& OutSlots) const override;

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        UIStackLayout Layout() const noexcept { return UIStackLayout{ Axis, Spacing, Padding, ChildAlign }; }

        /** Asked by SetScroll; GetScroll keeps it within the content, which may have changed since. */
        float m_Scroll = 0.f;

        bool     m_bDragging      = false;
        Vector2F m_DragFrom       = { 0.f, 0.f };   // the pointer when the drag began
        float    m_DragFromScroll = 0.f;
    };
}
