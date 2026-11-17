#include "UI/UIPose.h"

#include "UI/UIWidget.h"

namespace Opaax
{
    UIPose UIPose::Lerp(const UIPose& InFrom, const UIPose& InTo, const float InAlpha) noexcept
    {
        const auto lMix = [InAlpha](const float InA, const float InB) { return InA + (InB - InA) * InAlpha; };

        UIPose lPose;
        lPose.Translation = { lMix(InFrom.Translation.x, InTo.Translation.x), lMix(InFrom.Translation.y, InTo.Translation.y) };
        lPose.Scale       = { lMix(InFrom.Scale.x, InTo.Scale.x), lMix(InFrom.Scale.y, InTo.Scale.y) };
        lPose.Rotation    = lMix(InFrom.Rotation, InTo.Rotation);
        lPose.Opacity     = lMix(InFrom.Opacity, InTo.Opacity);
        lPose.Tint        = LinearColor(lMix(InFrom.Tint.r, InTo.Tint.r), lMix(InFrom.Tint.g, InTo.Tint.g),
                                        lMix(InFrom.Tint.b, InTo.Tint.b), lMix(InFrom.Tint.a, InTo.Tint.a));
        return lPose;
    }

    void UIPoseModifier::PostEditChange()
    {
        InvalidatePose();
    }

    void UIPoseModifier::OnAttach()
    {
        InvalidatePose();
    }

    void UIPoseModifier::OnDetach()
    {
        InvalidatePose();
    }

    void UIPoseModifier::InvalidatePose()
    {
        if (UIWidget* lOwner = GetOwner())
        {
            lOwner->InvalidatePose();
        }
    }
}
