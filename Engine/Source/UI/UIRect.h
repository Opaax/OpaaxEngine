#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // UIRect — how a widget sits in its parent (like Unity's RectTransform), Y-up: anchor (0,0)
    //   is the parent's bottom-left. Anchors follow the aspect ratio: a corner-anchored widget
    //   follows its corner; a 0..1 anchored one stretches.
    // =============================================================================
    struct UIRect
    {
        /**
         * Where in the parent, 0..1. Equal = a point; different = the widget stretches.
         * An inverted pair (Max below Min) is clamped.
         */
        Vector2F AnchorMin = { 0.5f, 0.5f };
        Vector2F AnchorMax = { 0.5f, 0.5f };

        /** Where in the widget the anchored position points, 0..1. */
        Vector2F Pivot = { 0.5f, 0.5f };

        /** Pivot offset from the anchor point, canvas units. */
        Vector2F AnchoredPosition = { 0.f, 0.f };

        /** Size minus the anchor rect's size (the full size for a point anchor). */
        Vector2F SizeDelta = { 100.f, 100.f };

        // _WITH_DEFAULT: a missing key keeps its default, so older .opaaxui files still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIRect,
                                                    AnchorMin, AnchorMax, Pivot, AnchoredPosition, SizeDelta)

        // Drag steps: fractions by 0.01, canvas units by 1.
        OPAAX_PROPERTIES(UIRect,
                         OPAAX_PROP(AnchorMin).SetRange(0.f, 1.f).SetDragStep(0.01f),
                         OPAAX_PROP(AnchorMax).SetRange(0.f, 1.f).SetDragStep(0.01f),
                         OPAAX_PROP(Pivot).SetRange(0.f, 1.f).SetDragStep(0.01f),
                         OPAAX_PROP(AnchoredPosition).SetDragStep(1.f),
                         OPAAX_PROP(SizeDelta).SetDragStep(1.f))
    };

    /**
     * The rect InRect gives inside InParent.
     */
    OPAAX_API Bounds2D ResolveRect(const UIRect& InRect, const Bounds2D& InParent) noexcept;

    /**
     * Inverse: sets InOutRect's SizeDelta and AnchoredPosition so ResolveRect gives InTarget.
     * Anchors and pivot are kept.
     */
    OPAAX_API void FitRect(UIRect& InOutRect, const Bounds2D& InTarget, const Bounds2D& InParent) noexcept;

    // =============================================================================
    // Anchor presets — Unity's 4x4 grid
    // =============================================================================

    enum class EUIAnchorX : Uint8 { Left, Center, Right, Stretch };
    enum class EUIAnchorY : Uint8 { Top, Middle, Bottom, Stretch };

    /** A preset per axis, or None when the anchors are not one of the sixteen. */
    struct UIAnchorPreset
    {
        EUIAnchorX X     = EUIAnchorX::Center;
        EUIAnchorY Y     = EUIAnchorY::Middle;
        bool       bKnown = false;
    };

    /**
     * Sets InOutRect's anchors and pivot to the preset, then fits it to InCurrent so the widget
     * does not move on screen. The pivot follows the anchor (0.5 on a stretched axis).
     */
    OPAAX_API void ApplyAnchorPreset(UIRect& InOutRect, EUIAnchorX InX, EUIAnchorY InY,
                                     const Bounds2D& InCurrent, const Bounds2D& InParent) noexcept;

    /** The preset InRect's anchors match; bKnown = false if none. */
    OPAAX_API UIAnchorPreset CurrentAnchorPreset(const UIRect& InRect) noexcept;
}
