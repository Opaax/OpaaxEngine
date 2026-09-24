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
        /**
         * Where in the parent, 0..1. Equal = a point anchor; different = the widget stretches.
         *
         * An INVERTED pair (Max below Min) is clamped at resolve rather than honoured — see
         * ResolveRect. Authoring one is easy (two drag fields, no ordering between them) and the
         * un-clamped result is a widget that silently moves somewhere else (**UI19**).
         */
        Vector2F AnchorMin = { 0.5f, 0.5f };
        Vector2F AnchorMax = { 0.5f, 0.5f };

        /** Where in the widget the anchored position points, 0..1. */
        Vector2F Pivot = { 0.5f, 0.5f };

        /** Pivot's offset from the anchor reference point, canvas units. */
        Vector2F AnchoredPosition = { 0.f, 0.f };

        /** Size minus the anchor rect's size — the full size for a point anchor. */
        Vector2F SizeDelta = { 100.f, 100.f };

        // _WITH_DEFAULT is required, not preferred: the plain macro reads with at(), which THROWS
        // on a missing key, so adding a field here would refuse every `.opaaxui` written before it.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIRect,
                                                    AnchorMin, AnchorMax, Pivot, AnchoredPosition, SizeDelta)

        // Fractions drag by a hundredth, canvas units by one — the step is what makes each editable.
        OPAAX_PROPERTIES(UIRect,
                         OPAAX_PROP(AnchorMin).SetRange(0.f, 1.f).SetDragStep(0.01f),
                         OPAAX_PROP(AnchorMax).SetRange(0.f, 1.f).SetDragStep(0.01f),
                         OPAAX_PROP(Pivot).SetRange(0.f, 1.f).SetDragStep(0.01f),
                         OPAAX_PROP(AnchoredPosition).SetDragStep(1.f),
                         OPAAX_PROP(SizeDelta).SetDragStep(1.f))
    };

    /**
     * The rect InRect describes inside InParent. Pure, so the maths is testable with nothing else built.
     */
    OPAAX_API Bounds2D ResolveRect(const UIRect& InRect, const Bounds2D& InParent) noexcept;

    /**
     * The inverse: InOutRect's SizeDelta and AnchoredPosition so that ResolveRect lands on InTarget
     * inside InParent. Anchors and pivot are kept — a resize handle edits a size, never an anchor.
     */
    OPAAX_API void FitRect(UIRect& InOutRect, const Bounds2D& InTarget, const Bounds2D& InParent) noexcept;

    // =============================================================================
    // Anchor presets — Unity's 4x4 grid, the way an author actually sets anchors (U12).
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
     * Set InOutRect's anchors AND pivot to the preset, then FitRect it to InCurrent inside
     * InParent — so the widget does not move on screen; only what it does on a resize changes.
     * The pivot follows the anchor (Unity's Shift+Alt click, made the only behaviour), 0.5 on a
     * stretched axis.
     */
    OPAAX_API void ApplyAnchorPreset(UIRect& InOutRect, EUIAnchorX InX, EUIAnchorY InY,
                                     const Bounds2D& InCurrent, const Bounds2D& InParent) noexcept;

    /** Which preset InRect's anchors are, for the highlight; bKnown = false for anything else. */
    OPAAX_API UIAnchorPreset CurrentAnchorPreset(const UIRect& InRect) noexcept;
}
