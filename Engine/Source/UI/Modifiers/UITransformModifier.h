#pragma once

#include "UI/UIPose.h"

namespace Opaax
{
    // =============================================================================
    // UITransformModifier — one pose that stays: a widget turned, scaled, moved, faded or tinted
    //   without touching its layout (Unreal's render transform). Its hit area follows by default.
    // =============================================================================
    class UITransformModifier final : public UIPoseModifier
    {
        OPAAX_OBJECT(UITransformModifier, UIPoseModifier)

    public:
        UIPose Pose;

        OPAAX_PROPERTIES(UITransformModifier, OPAAX_PROP(Pose).SetTooltip("The pose the widget is drawn with."))

        /** Sets the pose; the owner is drawn with it from its canvas's next update. */
        UITransformModifier& SetPose(const UIPose& InPose);

        UITransformModifier& SetPivot(const Vector2F& InPivot);

        UITransformModifier& SetMovesHitArea(bool bInMoves);

        //~Begin UIPoseModifier interface
        bool GetPose(UIPose& OutPose) const override;
        //~End UIPoseModifier interface
    };
}
