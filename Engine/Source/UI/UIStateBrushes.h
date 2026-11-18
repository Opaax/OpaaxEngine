#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/Easing.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "UI/UIBrush.h"
#include "UI/UIModifier.h"   // EUIInteractionState

namespace Opaax
{
    // =============================================================================
    // UIStateBrushes — what a widget draws in each interaction state, and how its tint moves between
    //   them. A state whose brush names no image shows Normal's image and border in its own tint, so
    //   one art with a colour per state takes one image (Unity's sprite swap falls back the same way).
    // =============================================================================
    struct UIStateBrushes
    {
        UIBrush Normal   = UIBrush::FromTint({ 0.25f, 0.25f, 0.28f, 1.f });
        UIBrush Hovered  = UIBrush::FromTint({ 0.35f, 0.35f, 0.40f, 1.f });
        UIBrush Pressed  = UIBrush::FromTint({ 0.15f, 0.15f, 0.18f, 1.f });
        UIBrush Focused  = UIBrush::FromTint({ 0.25f, 0.25f, 0.28f, 1.f });
        UIBrush Disabled = UIBrush::FromTint({ 0.20f, 0.20f, 0.20f, 0.5f });

        /** Seconds the tint takes to reach a new state's; 0 jumps. The image swaps at once. */
        float FadeDuration = 0.1f;

        /** The curve of the tint's fade. */
        EEase FadeEase = EEase::QuadOut;

        // _WITH_DEFAULT: a missing key keeps its default.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIStateBrushes, Normal, Hovered, Pressed, Focused, Disabled,
                                                    FadeDuration, FadeEase)

        OPAAX_PROPERTIES(UIStateBrushes,
                         OPAAX_PROP(Normal),
                         OPAAX_PROP(Hovered).SetTooltip("With no image, Normal's image and border show in this tint."),
                         OPAAX_PROP(Pressed).SetTooltip("With no image, Normal's image and border show in this tint."),
                         OPAAX_PROP(Focused).SetTooltip("With no image, Normal's image and border show in this tint."),
                         OPAAX_PROP(Disabled).SetTooltip("With no image, Normal's image and border show in this tint."),
                         OPAAX_PROP(FadeDuration).SetRange(0.f, 10.f).SetDragStep(0.01f)
                             .SetTooltip("Seconds the tint takes to reach a new state's. 0 jumps.\n"
                                         "The image always swaps at once."),
                         OPAAX_PROP(FadeEase).SetTooltip("The curve of the tint's fade."))

        /** The brush authored for InState, as it is. */
        const UIBrush& Get(EUIInteractionState InState) const noexcept;

        /** What InState draws: its brush, or, when that names no image, Normal's image and border in its tint. */
        UIBrush For(EUIInteractionState InState) const;

        /** Keeps FadeDuration at 0 or more. */
        void Sanitize() noexcept;
    };

    /**
     * The tint shown InElapsed seconds into a fade from InFrom to InTo over InDuration with InEase: InTo
     * once it is over (and at once when InDuration is 0).
     */
    LinearColor FadeTint(const LinearColor& InFrom, const LinearColor& InTo, float InElapsed, float InDuration,
                         EEase InEase) noexcept;
}
