#pragma once

#include "Core/Maths/Easing.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/Asset.h"
#include "UI/UIModifier.h"
#include "UI/UIPose.h"

namespace Opaax
{
    // =============================================================================
    // UITransition — a transition's feel as an asset (a "UI Transition" preset): how long, along which
    //   curve, and the pose of each interaction state. Many buttons name one and share it; saving it
    //   reaches every one of them, playing or not. Normal is the rest pose.
    // =============================================================================
    class UITransition final : public Asset
    {
        OPAAX_OBJECT(UITransition, Asset)

    public:
        /** Seconds from one state's pose to the next. 0 jumps. */
        float  Duration = 0.12f;
        EEase  Ease     = EEase::QuadOut;
        UIPose Hovered;
        UIPose Pressed;
        UIPose Focused;
        UIPose Disabled;

        OPAAX_PROPERTIES(UITransition,
                         OPAAX_PROP(Duration).SetRange(0.f, 10.f).SetDragStep(0.01f)
                                             .SetTooltip("Seconds from one state's pose to the next. 0 jumps."),
                         OPAAX_PROP(Ease).SetTooltip("The curve of the motion."),
                         OPAAX_PROP(Hovered).SetTooltip("The pose under the pointer."),
                         OPAAX_PROP(Pressed).SetTooltip("The pose while held (and still under the pointer)."),
                         OPAAX_PROP(Focused).SetTooltip("The pose while it takes the keys."),
                         OPAAX_PROP(Disabled).SetTooltip("The pose while it takes no input (a disabled button)."))

        /** The pose an interaction state asks for (Normal: the rest pose). */
        const UIPose& PoseFor(EUIInteractionState InState) const noexcept;

        //~Begin Object interface
        /** Keeps Duration at 0 or more. */
        void PostEditChange() override;
        //~End Object interface
    };
}
