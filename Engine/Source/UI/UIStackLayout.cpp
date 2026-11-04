#include "UI/UIStackLayout.h"

#include "Core/Maths/Maths.h"

namespace Opaax
{
    float UIStackLayout::ContentExtent(const TDynArray<Vector2F>& InSizes) const noexcept
    {
        const bool lVertical = Axis == EUIAxis::Vertical;

        float lTotal = lVertical ? Padding.Top + Padding.Bottom : Padding.Left + Padding.Right;

        for (Uint64 lIndex = 0; lIndex < InSizes.size(); ++lIndex)
        {
            lTotal += Maths::Max(lVertical ? InSizes[lIndex].y : InSizes[lIndex].x, 0.f);
            if (lIndex > 0) { lTotal += Spacing; }
        }

        return lTotal;
    }

    void UIStackLayout::Arrange(const Bounds2D& InBounds, const TDynArray<Vector2F>& InSizes, const float InScroll,
                                TDynArray<Bounds2D>& OutSlots) const
    {
        // The inner rect, padding removed; a padding wider than the rect collapses it.
        const Vector2F lMin{ InBounds.Min().x + Padding.Left,  InBounds.Min().y + Padding.Bottom };
        const Vector2F lMax{ Maths::Max(InBounds.Max().x - Padding.Right, lMin.x),
                             Maths::Max(InBounds.Max().y - Padding.Top,   lMin.y) };

        // Scrolling moves the content toward the start: up (Y-up), or left.
        const bool lVertical = Axis == EUIAxis::Vertical;
        float      lCursor   = lVertical ? lMax.y + InScroll : lMin.x - InScroll;   // top, or left

        OutSlots.reserve(OutSlots.size() + InSizes.size());

        for (const Vector2F& lDesired : InSizes)
        {
            const Vector2F lSize{ Maths::Max(lDesired.x, 0.f), Maths::Max(lDesired.y, 0.f) };

            if (lVertical)
            {
                // Down the axis; across it, Start is the left.
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
                // Along the axis; across it, Start is the top (Y-up).
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
