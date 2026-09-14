#include "UI/UIRect.h"

namespace Opaax
{
    Bounds2D ResolveRect(const UIRect& InRect, const Bounds2D& InParent) noexcept
    {
        const Vector2F lParentMin  = InParent.Min();
        const Vector2F lParentSize = InParent.Size();

        const Vector2F lAnchorMin  = lParentMin + InRect.AnchorMin * lParentSize;
        const Vector2F lAnchorSize = (InRect.AnchorMax - InRect.AnchorMin) * lParentSize;

        const Vector2F lSize     = lAnchorSize + InRect.SizeDelta;
        const Vector2F lPivotPos = lAnchorMin + InRect.Pivot * lAnchorSize + InRect.AnchoredPosition;
        const Vector2F lMin      = lPivotPos - InRect.Pivot * lSize;

        return Bounds2D::FromMinMax(lMin, lMin + lSize);
    }
}
