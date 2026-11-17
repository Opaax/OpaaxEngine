#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Maths/Affine2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"
#include "UI/UIModifier.h"

namespace Opaax
{
    // =============================================================================
    // UIPose — how a widget is drawn away from its layout: moved, scaled and turned about a pivot,
    //   faded and tinted. Never changes the layout or a saved field of the widget; applies to the
    //   widget and everything under it. The default is the rest pose (changes nothing).
    // =============================================================================
    struct UIPose
    {
        /** Canvas units. */
        Vector2F    Translation = { 0.f, 0.f };
        Vector2F    Scale       = { 1.f, 1.f };
        /** Degrees, counter-clockwise. */
        float       Rotation    = 0.f;
        /** Times the widget's own Opacity. */
        float       Opacity     = 1.f;
        /** Times its quads' colours. */
        LinearColor Tint;

        /** True when the pose changes nothing. */
        bool IsRest() const noexcept
        {
            return Translation.x == 0.f && Translation.y == 0.f && Scale.x == 1.f && Scale.y == 1.f
                && Rotation == 0.f && Opacity == 1.f
                && Tint.r == 1.f && Tint.g == 1.f && Tint.b == 1.f && Tint.a == 1.f;
        }

        /**
         * The pose InAlpha of the way from InFrom to InTo, each field on a line (InAlpha may leave [0, 1]:
         * Back and Elastic overshoot). Rotation is blended as written: 0 to 360 is a whole turn.
         */
        static UIPose Lerp(const UIPose& InFrom, const UIPose& InTo, float InAlpha) noexcept;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIPose, Translation, Scale, Rotation, Opacity, Tint)

        OPAAX_PROPERTIES(UIPose,
                         OPAAX_PROP(Translation).SetDragStep(1.f).SetTooltip("Moves the widget, in canvas units."),
                         OPAAX_PROP(Scale).SetDragStep(0.01f).SetTooltip("Scales the widget about the pivot."),
                         OPAAX_PROP(Rotation).SetDragStep(0.5f)
                                             .SetTooltip("Turns the widget about the pivot, degrees counter-clockwise."),
                         OPAAX_PROP(Opacity).SetRange(0.f, 1.f).SetDragStep(0.01f)
                                            .SetTooltip("Times the widget's own Opacity."),
                         OPAAX_PROP(Tint).SetTooltip("Times the colour of everything the widget draws."))
    };

    /**
     * A widget's poses composed, in canvas space: what its draw walk and its hit test apply. Not saved.
     */
    struct UIComposedPose
    {
        /** Every enabled pose, in modifier order, each about its own pivot. */
        Affine2D Draw;
        /** Only the poses that move the hit area (bMovesHitArea). */
        Affine2D Hit;
        float    Opacity = 1.f;
        Vector4F Tint    = { 1.f, 1.f, 1.f, 1.f };
        /** Nothing posed: the draw walk takes the fast way. */
        bool     bRest   = true;
    };

    // =============================================================================
    // UIPoseModifier — the base of the modifiers that give a pose. The owner composes them in list
    //   order: each pose about its own Pivot, on top of the earlier ones; opacity and tint multiply.
    //   A subclass answers GetPose and calls InvalidatePose when the answer changes.
    // =============================================================================
    class UIPoseModifier : public UIModifier
    {
        OPAAX_ABSTRACT_OBJECT(UIPoseModifier, UIModifier)

    public:
        /** The point the pose scales and turns about, in the widget's layout rect (0..1). */
        Vector2F Pivot         = { 0.5f, 0.5f };
        /** On: clicks follow the posed widget. Off: they keep hitting its layout rect. */
        bool     bMovesHitArea = true;

        OPAAX_PROPERTIES(UIPoseModifier,
                         OPAAX_PROP(Pivot).SetRange(0.f, 1.f).SetDragStep(0.01f)
                                          .SetTooltip("The point the pose scales and turns about,\n"
                                                      "in the widget's rect: 0,0 bottom-left, 1,1 top-right."),
                         OPAAX_PROP(bMovesHitArea).SetTooltip("On: clicks follow the posed widget.\n"
                                                              "Off: they hit its layout rect."))

        /**
         * The pose now.
         * @return False at rest: nothing to apply
         */
        virtual bool GetPose(UIPose& OutPose) const { (void)OutPose; return false; }

        //~Begin Object interface
        /** An edited field (the editor writes them directly) poses the owner again. */
        void PostEditChange() override;
        //~End Object interface

    protected:
        //~Begin UIModifier interface
        void OnAttach() override;
        void OnDetach() override;
        //~End UIModifier interface

        /** The owner composes its poses again before it is next drawn. */
        void InvalidatePose();
    };
}
