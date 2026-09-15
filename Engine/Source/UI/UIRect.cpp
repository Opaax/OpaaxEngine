#include "UI/UIRect.h"

#include <algorithm>   // std::max — the inverted-anchor clamp

namespace Opaax
{
    namespace
    {
        /** The anchor rect InRect spans inside InParent — what both directions are measured from. */
        struct AnchorFrame
        {
            Vector2F Min;
            Vector2F Size;
        };

        AnchorFrame MakeAnchorFrame(const UIRect& InRect, const Bounds2D& InParent) noexcept
        {
            const Vector2F lParentMin  = InParent.Min();
            const Vector2F lParentSize = InParent.Size();

            // AN INVERTED ANCHOR PAIR IS CLAMPED, NOT HONOURED (**UI19**). AnchorMax below AnchorMin
            // gives a NEGATIVE anchor size, which flows into a negative widget size, and Bounds2D's
            // FromMinMax then silently SORTS the corners — so the widget lands somewhere plausible and
            // wrong instead of anywhere it was put. Clamping here fixes it for every caller at once,
            // because this is the one place a rect's anchors are ever measured.
            //
            // Only the INVERSION is clamped: anchors outside 0..1 are legitimate (Unity allows them)
            // and do not produce the negative size this exists to stop.
            const Vector2F lAnchorMax{ std::max(InRect.AnchorMin.x, InRect.AnchorMax.x),
                                       std::max(InRect.AnchorMin.y, InRect.AnchorMax.y) };

            return { lParentMin + InRect.AnchorMin * lParentSize,
                     (lAnchorMax - InRect.AnchorMin) * lParentSize };
        }
    }

    Bounds2D ResolveRect(const UIRect& InRect, const Bounds2D& InParent) noexcept
    {
        const AnchorFrame lAnchor = MakeAnchorFrame(InRect, InParent);

        const Vector2F lSize     = lAnchor.Size + InRect.SizeDelta;
        const Vector2F lPivotPos = lAnchor.Min + InRect.Pivot * lAnchor.Size + InRect.AnchoredPosition;
        const Vector2F lMin      = lPivotPos - InRect.Pivot * lSize;

        return Bounds2D::FromMinMax(lMin, lMin + lSize);
    }

    void FitRect(UIRect& InOutRect, const Bounds2D& InTarget, const Bounds2D& InParent) noexcept
    {
        const AnchorFrame lAnchor = MakeAnchorFrame(InOutRect, InParent);
        const Vector2F    lSize   = InTarget.Size();

        InOutRect.SizeDelta        = lSize - lAnchor.Size;
        InOutRect.AnchoredPosition = InTarget.Min() + InOutRect.Pivot * lSize
                                   - (lAnchor.Min + InOutRect.Pivot * lAnchor.Size);
    }
}
