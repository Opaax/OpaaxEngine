#include "UI/UIRect.h"

#include <algorithm>   // std::max — the inverted-anchor clamp

namespace Opaax
{
    Bounds2D ResolveRect(const UIRect& InRect, const Bounds2D& InParent) noexcept
    {
        const Vector2F lParentMin  = InParent.Min();
        const Vector2F lParentSize = InParent.Size();

        // AN INVERTED ANCHOR PAIR IS CLAMPED, NOT HONOURED (**UI19**). AnchorMax below AnchorMin
        // gives a NEGATIVE anchor size, which flows into a negative widget size, and Bounds2D's
        // FromMinMax then silently SORTS the corners — so the widget lands somewhere plausible and
        // wrong instead of anywhere it was put. Clamping here fixes it for every caller at once,
        // because this is the one function a rect is ever resolved through.
        //
        // Only the INVERSION is clamped: anchors outside 0..1 are legitimate (Unity allows them)
        // and do not produce the negative size this exists to stop.
        const Vector2F lAnchorMax{ std::max(InRect.AnchorMin.x, InRect.AnchorMax.x),
                                   std::max(InRect.AnchorMin.y, InRect.AnchorMax.y) };

        const Vector2F lAnchorMin  = lParentMin + InRect.AnchorMin * lParentSize;
        const Vector2F lAnchorSize = (lAnchorMax - InRect.AnchorMin) * lParentSize;

        const Vector2F lSize     = lAnchorSize + InRect.SizeDelta;
        const Vector2F lPivotPos = lAnchorMin + InRect.Pivot * lAnchorSize + InRect.AnchoredPosition;
        const Vector2F lMin      = lPivotPos - InRect.Pivot * lSize;

        return Bounds2D::FromMinMax(lMin, lMin + lSize);
    }
}
