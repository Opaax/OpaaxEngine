#include "UI/Widgets/UIStack.h"

#include "Core/Reflection/OpaaxEnumJson.h"

namespace Opaax
{
    UIStack::UIStack()
    {
        bHitTestable        = false;
        m_bArrangesChildren = true;
    }

    // =============================================================================
    // Authored state — every change moves the children, so each one invalidates the layout
    // =============================================================================

    void UIStack::SetAxis(const EUIAxis InAxis)
    {
        Axis = InAxis;
        InvalidateLayout();
    }

    void UIStack::SetSpacing(const float InSpacing)
    {
        Spacing = InSpacing;
        InvalidateLayout();
    }

    void UIStack::SetPadding(const UIMargin& InPadding)
    {
        Padding = InPadding;
        InvalidateLayout();
    }

    void UIStack::SetChildAlign(const EUIAlign InAlign)
    {
        ChildAlign = InAlign;
        InvalidateLayout();
    }

    void UIStack::SetFitContent(const bool bInFit)
    {
        bFitContent = bInFit;
        InvalidateLayout();
    }

    // =============================================================================
    // UIWidget
    // =============================================================================

    Bounds2D UIStack::ResolveBounds(const Bounds2D& InParentBounds) const
    {
        if (!bFitContent)
        {
            return ResolveRect(Rect, InParentBounds);
        }

        // Only a point-anchored axis can fit the content; a stretched one is sized by the parent.
        const float lExtent = Layout().ContentExtent(UIStackLayout::DesiredSizes(GetChildren()));

        UIRect lRect = Rect;
        if (Axis == EUIAxis::Vertical)
        {
            if (lRect.AnchorMin.y == lRect.AnchorMax.y) { lRect.SizeDelta.y = lExtent; }
        }
        else
        {
            if (lRect.AnchorMin.x == lRect.AnchorMax.x) { lRect.SizeDelta.x = lExtent; }
        }

        return ResolveRect(lRect, InParentBounds);
    }

    void UIStack::ArrangeChildren(TDynArray<Bounds2D>& OutSlots) const
    {
        Layout().Arrange(GetBounds(), UIStackLayout::DesiredSizes(GetChildren()), 0.f, OutSlots);
    }
}
