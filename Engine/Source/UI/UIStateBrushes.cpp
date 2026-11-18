#include "UI/UIStateBrushes.h"

#include "Core/Maths/Maths.h"

namespace Opaax
{
    const UIBrush& UIStateBrushes::Get(const EUIInteractionState InState) const noexcept
    {
        switch (InState)
        {
            case EUIInteractionState::Hovered:  return Hovered;
            case EUIInteractionState::Pressed:  return Pressed;
            case EUIInteractionState::Focused:  return Focused;
            case EUIInteractionState::Disabled: return Disabled;
            case EUIInteractionState::Normal:   break;
        }
        return Normal;
    }

    UIBrush UIStateBrushes::For(const EUIInteractionState InState) const
    {
        const UIBrush& lOwn = Get(InState);
        if (lOwn.HasImage())
        {
            return lOwn;
        }

        // Normal's image and border, this state's tint.
        UIBrush lBrush = Normal;
        lBrush.Tint    = lOwn.Tint;
        return lBrush;
    }

    void UIStateBrushes::Sanitize() noexcept
    {
        FadeDuration = Maths::Max(FadeDuration, 0.f);
    }

    LinearColor FadeTint(const LinearColor& InFrom, const LinearColor& InTo, const float InElapsed,
                         const float InDuration, const EEase InEase) noexcept
    {
        if (InDuration <= 0.f || InElapsed >= InDuration)
        {
            return InTo;
        }

        const float lT = Easing::Evaluate(InEase, InElapsed / InDuration);
        return LinearColor(InFrom.x + (InTo.x - InFrom.x) * lT, InFrom.y + (InTo.y - InFrom.y) * lT,
                           InFrom.z + (InTo.z - InFrom.z) * lT, InFrom.w + (InTo.w - InFrom.w) * lT);
    }
}
