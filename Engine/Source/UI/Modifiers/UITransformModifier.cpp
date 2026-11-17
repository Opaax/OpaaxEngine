#include "UI/Modifiers/UITransformModifier.h"

namespace Opaax
{
    UITransformModifier& UITransformModifier::SetPose(const UIPose& InPose)
    {
        Pose = InPose;
        InvalidatePose();
        return *this;
    }

    UITransformModifier& UITransformModifier::SetPivot(const Vector2F& InPivot)
    {
        Pivot = InPivot;
        InvalidatePose();
        return *this;
    }

    UITransformModifier& UITransformModifier::SetMovesHitArea(const bool bInMoves)
    {
        bMovesHitArea = bInMoves;
        InvalidatePose();
        return *this;
    }

    bool UITransformModifier::GetPose(UIPose& OutPose) const
    {
        if (Pose.IsRest())
        {
            return false;
        }

        OutPose = Pose;
        return true;
    }
}
