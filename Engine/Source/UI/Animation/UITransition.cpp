#include "UI/Animation/UITransition.h"

namespace Opaax
{
    namespace
    {
        const UIPose REST_POSE{};
    }

    const UIPose& UITransition::PoseFor(const EUIInteractionState InState) const noexcept
    {
        switch (InState)
        {
        case EUIInteractionState::Hovered:  return Hovered;
        case EUIInteractionState::Pressed:  return Pressed;
        case EUIInteractionState::Focused:  return Focused;
        case EUIInteractionState::Disabled: return Disabled;
        case EUIInteractionState::Normal:   break;
        }
        return REST_POSE;
    }

    void UITransition::PostEditChange()
    {
        if (Duration < 0.f)
        {
            Duration = 0.f;
        }
    }
}
