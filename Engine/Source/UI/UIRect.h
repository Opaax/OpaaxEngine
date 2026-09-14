#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // UIRect — how a widget sits inside its parent. Unity's RectTransform fields, Y-UP like the
    //   world: anchor (0,0) is the parent's bottom-left. The RESOLVED rect is a Bounds2D, which
    //   is what the renderer draws and what a hit-test asks.
    //
    //   Anchors absorb the aspect ratio: a widget anchored to a corner follows that corner when
    //   the visible canvas widens, one anchored 0..1 stretches with it (CAM2 — width follows the
    //   target; the canvas never sees pixels).
    // =============================================================================
    struct UIRect
    {
        /** Where in the parent, 0..1. Equal = a point anchor; different = the widget stretches. */
        Vector2F AnchorMin = { 0.5f, 0.5f };
        Vector2F AnchorMax = { 0.5f, 0.5f };

        /** Where in the widget the anchored position points, 0..1. */
        Vector2F Pivot = { 0.5f, 0.5f };

        /** Pivot's offset from the anchor reference point, canvas units. */
        Vector2F AnchoredPosition = { 0.f, 0.f };

        /** Size minus the anchor rect's size — the full size for a point anchor. */
        Vector2F SizeDelta = { 100.f, 100.f };

        OPAAX_PROPERTIES(UIRect,
                         OPAAX_PROP(AnchorMin).SetRange(0.f, 1.f),
                         OPAAX_PROP(AnchorMax).SetRange(0.f, 1.f),
                         OPAAX_PROP(Pivot).SetRange(0.f, 1.f),
                         OPAAX_PROP(AnchoredPosition),
                         OPAAX_PROP(SizeDelta))
    };

    /**
     * The rect InRect describes inside InParent. Pure, so the maths is testable with nothing else built.
     */
    OPAAX_API Bounds2D ResolveRect(const UIRect& InRect, const Bounds2D& InParent) noexcept;
}
