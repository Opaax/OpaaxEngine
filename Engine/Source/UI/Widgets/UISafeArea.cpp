#include "UI/Widgets/UISafeArea.h"

#include <algorithm>

namespace Opaax
{
    namespace
    {
        /** A fraction per edge, sane for any authored pair: never negative, never more than the rect. */
        void FitInsets(float& InOutLow, float& InOutHigh) noexcept
        {
            InOutLow  = std::max(0.f, InOutLow);
            InOutHigh = std::max(0.f, InOutHigh);

            // At 1.0 the two edges meet and the rect vanishes; keep a sliver so a mis-typed inset
            // leaves something on screen to fix rather than a widget that disappeared.
            constexpr float lMaxSum = 0.9f;

            const float lSum = InOutLow + InOutHigh;
            if (lSum > lMaxSum)
            {
                const float lScale = lMaxSum / lSum;
                InOutLow  *= lScale;
                InOutHigh *= lScale;
            }
        }
    }

    UISafeArea::UISafeArea()
    {
        bHitTestable = false;

        Rect.AnchorMin = { 0.f, 0.f };
        Rect.AnchorMax = { 1.f, 1.f };
        Rect.SizeDelta = { 0.f, 0.f };
    }

    void UISafeArea::SetInsets(const UIMargin& InInsets)
    {
        Insets = InInsets;

        // My rect changes, so my whole subtree re-resolves — the insets are what children anchor to.
        InvalidateLayout();
    }

    Bounds2D UISafeArea::ResolveBounds(const Bounds2D& InParentBounds) const
    {
        const Bounds2D lFull = ResolveRect(Rect, InParentBounds);

        float lLeft = Insets.Left, lRight = Insets.Right;
        FitInsets(lLeft, lRight);

        float lBottom = Insets.Bottom, lTop = Insets.Top;
        FitInsets(lBottom, lTop);

        const Vector2F lMin  = lFull.Min();
        const Vector2F lSize = lFull.Size();

        return Bounds2D::FromMinMax({ lMin.x + lSize.x * lLeft, lMin.y + lSize.y * lBottom },
                                    { lMin.x + lSize.x * (1.f - lRight), lMin.y + lSize.y * (1.f - lTop) });
    }

    void UISafeArea::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        InOutJson["Insets"] = Insets;
    }

    void UISafeArea::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Insets = InJson.value("Insets", Insets);
    }
}
