#include "UI/Modifiers/UITransitionModifier.h"

#include "Core/Maths/Maths.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    namespace
    {
        const UIPose REST_POSE{};
    }

    const UIPose& UITransitionModifier::PoseFor(const EUIInteractionState InState) const noexcept
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

    bool UITransitionModifier::GetPose(UIPose& OutPose) const
    {
        if (m_Shown.IsRest())
        {
            return false;
        }

        OutPose = m_Shown;
        return true;
    }

    void UITransitionModifier::PostEditChange()
    {
        // An edited pose shows at once: the designer sees the value they set.
        m_Shown   = PoseFor(m_State);
        m_Elapsed = Duration;
        UIPoseModifier::PostEditChange();
    }

    void UITransitionModifier::OnAttach()
    {
        // Joins at its owner's state, without a motion (a button loaded disabled shows Disabled).
        m_State   = GetOwner() != nullptr ? GetOwner()->GetInteractionState() : EUIInteractionState::Normal;
        m_Shown   = PoseFor(m_State);
        m_Elapsed = Duration;
        UIPoseModifier::OnAttach();
    }

    void UITransitionModifier::OnStateChanged(const EUIInteractionState InFrom, const EUIInteractionState InTo)
    {
        (void)InFrom;
        if (InTo == m_State)
        {
            return;
        }

        // From the pose shown now: an interrupted transition goes on from where it is.
        m_State   = InTo;
        m_From    = m_Shown;
        m_Elapsed = 0.f;

        if (Duration <= 0.f)
        {
            m_Shown = PoseFor(m_State);
            InvalidatePose();
            return;
        }
        WakeUp();
    }

    bool UITransitionModifier::Tick(const float InSeconds)
    {
        m_Elapsed += InSeconds;

        const float lTime = Duration > 0.f ? Maths::Min(m_Elapsed / Duration, 1.f) : 1.f;
        m_Shown = lTime >= 1.f ? PoseFor(m_State) : UIPose::Lerp(m_From, PoseFor(m_State), Easing::Evaluate(Ease, lTime));
        InvalidatePose();

        return lTime < 1.f;
    }
}
