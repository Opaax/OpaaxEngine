#include "UI/Widgets/UIStack.h"

#include <algorithm>

#include "Core/Reflection/OpaaxEnumJson.h"

namespace Opaax
{
    UIStack::UIStack()
    {
        bHitTestable        = false;
        m_bArrangesChildren = true;
    }

    // =============================================================================
    // Authored state — every one of these moves every child, so all of them owe a layout
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

    void UIStack::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        InOutJson["Axis"]        = Axis;
        InOutJson["Spacing"]     = Spacing;
        InOutJson["Padding"]     = Padding;
        InOutJson["ChildAlign"]  = ChildAlign;
        InOutJson["bFitContent"] = bFitContent;
    }

    void UIStack::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Axis        = InJson.value("Axis", Axis);
        Spacing     = InJson.value("Spacing", Spacing);
        Padding     = InJson.value("Padding", Padding);
        ChildAlign  = InJson.value("ChildAlign", ChildAlign);
        bFitContent = InJson.value("bFitContent", bFitContent);
    }

    float UIStack::ContentExtent() const noexcept
    {
        const bool lVertical = Axis == EUIAxis::Vertical;

        float lTotal = lVertical ? Padding.Top + Padding.Bottom : Padding.Left + Padding.Right;

        const TDynArray<TUniquePtr<UIWidget>>& lChildren = GetChildren();
        for (Uint64 lIndex = 0; lIndex < lChildren.size(); ++lIndex)
        {
            const Vector2F& lSize = lChildren[lIndex]->Rect.SizeDelta;
            lTotal += std::max(lVertical ? lSize.y : lSize.x, 0.f);
            if (lIndex > 0) { lTotal += Spacing; }
        }

        return lTotal;
    }

    Bounds2D UIStack::ResolveBounds(const Bounds2D& InParentBounds) const
    {
        if (!bFitContent)
        {
            return ResolveRect(Rect, InParentBounds);
        }

        // Only a POINT-anchored axis can follow the content; a stretched one is the parent's to size.
        UIRect lRect = Rect;
        if (Axis == EUIAxis::Vertical)
        {
            if (lRect.AnchorMin.y == lRect.AnchorMax.y) { lRect.SizeDelta.y = ContentExtent(); }
        }
        else
        {
            if (lRect.AnchorMin.x == lRect.AnchorMax.x) { lRect.SizeDelta.x = ContentExtent(); }
        }

        return ResolveRect(lRect, InParentBounds);
    }

    void UIStack::ArrangeChildren(TDynArray<Bounds2D>& OutSlots) const
    {
        const Bounds2D& lBounds = GetBounds();

        // The inner rect, padding taken off each edge; a padding wider than the rect collapses it.
        const Vector2F lMin{ lBounds.Min().x + Padding.Left,  lBounds.Min().y + Padding.Bottom };
        const Vector2F lMax{ std::max(lBounds.Max().x - Padding.Right, lMin.x),
                             std::max(lBounds.Max().y - Padding.Top,   lMin.y) };

        const bool lVertical = Axis == EUIAxis::Vertical;
        float      lCursor   = lVertical ? lMax.y : lMin.x;   // the top, or the left

        OutSlots.reserve(GetChildren().size());

        for (const TUniquePtr<UIWidget>& lChild : GetChildren())
        {
            const Vector2F lSize{ std::max(lChild->Rect.SizeDelta.x, 0.f), std::max(lChild->Rect.SizeDelta.y, 0.f) };

            if (lVertical)
            {
                // Down the axis; across it, Start is the LEFT.
                float lLeft = lMin.x, lRight = lMax.x;
                switch (ChildAlign)
                {
                    case EUIAlign::Start:   lRight = lMin.x + lSize.x; break;
                    case EUIAlign::End:     lLeft  = lMax.x - lSize.x; break;
                    case EUIAlign::Center:  lLeft  = (lMin.x + lMax.x - lSize.x) * 0.5f; lRight = lLeft + lSize.x; break;
                    case EUIAlign::Stretch: break;
                }

                OutSlots.emplace_back(Bounds2D::FromMinMax({ lLeft, lCursor - lSize.y }, { lRight, lCursor }));
                lCursor -= lSize.y + Spacing;
            }
            else
            {
                // Along the axis; across it, Start is the TOP (the canvas is Y-up).
                float lBottom = lMin.y, lTop = lMax.y;
                switch (ChildAlign)
                {
                    case EUIAlign::Start:   lBottom = lMax.y - lSize.y; break;
                    case EUIAlign::End:     lTop    = lMin.y + lSize.y; break;
                    case EUIAlign::Center:  lBottom = (lMin.y + lMax.y - lSize.y) * 0.5f; lTop = lBottom + lSize.y; break;
                    case EUIAlign::Stretch: break;
                }

                OutSlots.emplace_back(Bounds2D::FromMinMax({ lCursor, lBottom }, { lCursor + lSize.x, lTop }));
                lCursor += lSize.x + Spacing;
            }
        }
    }
}
