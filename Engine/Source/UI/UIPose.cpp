#include "UI/UIPose.h"

#include "UI/UIWidget.h"

namespace Opaax
{
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
