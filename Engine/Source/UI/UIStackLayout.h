#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "UI/UIMargin.h"

namespace Opaax
{
    /** Stack direction. */
    enum class EUIAxis : Uint8
    {
        Vertical,     // top to bottom
        Horizontal    // left to right
    };

    inline const char* ToString(const EUIAxis InAxis) noexcept
    {
        return InAxis == EUIAxis::Horizontal ? "Horizontal" : "Vertical";
    }

    /** Placement across the stack's axis. Start is left (vertical) or top (horizontal). */
    enum class EUIAlign : Uint8
    {
        Start,
        Center,
        End,
        Stretch   // fills the cross axis
    };

    inline const char* ToString(const EUIAlign InAlign) noexcept
    {
        switch (InAlign)
        {
            case EUIAlign::Start:   return "Start";
            case EUIAlign::Center:  return "Center";
            case EUIAlign::End:     return "End";
            case EUIAlign::Stretch: return "Stretch";
        }
        return "Start";
    }

    // =============================================================================
    // UIStackLayout — children one after another along an axis, the layout UIStack and UIScrollBox
    //   share. Pure: sizes in, slots out.
    // =============================================================================
    struct UIStackLayout
    {
        EUIAxis  Axis       = EUIAxis::Vertical;
        float    Spacing    = 0.f;     // between children, canvas units
        UIMargin Padding;              // inside the bounds, canvas units
        EUIAlign ChildAlign = EUIAlign::Center;

        /** The size along the axis that InSizes take, spacing and padding included. */
        float ContentExtent(const TDynArray<Vector2F>& InSizes) const noexcept;

        /**
         * One slot per size inside InBounds, from the axis's start (the top, or the left), moved InScroll
         * along the axis: what a scroll box has scrolled past. A negative size counts as 0.
         */
        void Arrange(const Bounds2D& InBounds, const TDynArray<Vector2F>& InSizes, float InScroll,
                     TDynArray<Bounds2D>& OutSlots) const;

        /** The desired sizes of InChildren (their Rect.SizeDelta), in order. */
        template<typename TChildren>
        static TDynArray<Vector2F> DesiredSizes(const TChildren& InChildren)
        {
            TDynArray<Vector2F> lSizes;
            lSizes.reserve(InChildren.size());
            for (const auto& lChild : InChildren)
            {
                lSizes.emplace_back(lChild->Rect.SizeDelta);
            }
            return lSizes;
        }
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIAxis, Vertical, Horizontal);
OPAAX_ENUM_VALUES(Opaax::EUIAlign, Start, Center, End, Stretch);
