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

    // =============================================================================
    // Anchor presets
    // =============================================================================

    namespace
    {
        /** One axis of a preset as (min, max) anchors; the pivot is the min for a point, 0.5 stretched. */
        void AxisAnchors(const Uint8 InPreset, const bool bInStretch, float& OutMin, float& OutMax) noexcept
        {
            if (bInStretch)   { OutMin = 0.f;  OutMax = 1.f;  return; }
            const float lAt = InPreset == 0 ? 0.f : InPreset == 1 ? 0.5f : 1.f;
            OutMin = lAt;
            OutMax = lAt;
        }
    }

    void ApplyAnchorPreset(UIRect& InOutRect, const EUIAnchorX InX, const EUIAnchorY InY,
                           const Bounds2D& InCurrent, const Bounds2D& InParent) noexcept
    {
        // X reads left-to-right as authored; Y is listed top-down for the author but the canvas is
        // Y-UP, so Top is anchor 1 and Bottom is anchor 0.
        AxisAnchors(static_cast<Uint8>(InX), InX == EUIAnchorX::Stretch, InOutRect.AnchorMin.x, InOutRect.AnchorMax.x);

        const Uint8 lYFlipped = InY == EUIAnchorY::Top ? 2 : InY == EUIAnchorY::Middle ? 1 : 0;
        AxisAnchors(lYFlipped, InY == EUIAnchorY::Stretch, InOutRect.AnchorMin.y, InOutRect.AnchorMax.y);

        InOutRect.Pivot = { InX == EUIAnchorX::Stretch ? 0.5f : InOutRect.AnchorMin.x,
                            InY == EUIAnchorY::Stretch ? 0.5f : InOutRect.AnchorMin.y };

        FitRect(InOutRect, InCurrent, InParent);
    }

    UIAnchorPreset CurrentAnchorPreset(const UIRect& InRect) noexcept
    {
        const auto lAxis = [](const float InMin, const float InMax, Uint8& OutPreset, bool& OutStretch)
        {
            if (InMin == 0.f && InMax == 1.f) { OutStretch = true; OutPreset = 3; return true; }
            if (InMin != InMax)               { return false; }
            OutStretch = false;
            if (InMin == 0.f)   { OutPreset = 0; return true; }
            if (InMin == 0.5f)  { OutPreset = 1; return true; }
            if (InMin == 1.f)   { OutPreset = 2; return true; }
            return false;
        };

        UIAnchorPreset lPreset;
        Uint8 lX = 0, lY = 0;
        bool  lStretchX = false, lStretchY = false;

        if (!lAxis(InRect.AnchorMin.x, InRect.AnchorMax.x, lX, lStretchX)
         || !lAxis(InRect.AnchorMin.y, InRect.AnchorMax.y, lY, lStretchY))
        {
            return lPreset;   // bKnown = false
        }

        lPreset.X      = lStretchX ? EUIAnchorX::Stretch : static_cast<EUIAnchorX>(lX);
        lPreset.Y      = lStretchY ? EUIAnchorY::Stretch : lY == 2 ? EUIAnchorY::Top : lY == 1 ? EUIAnchorY::Middle : EUIAnchorY::Bottom;
        lPreset.bKnown = true;
        return lPreset;
    }
}
