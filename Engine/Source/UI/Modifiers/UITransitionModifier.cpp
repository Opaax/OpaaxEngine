#include "UI/Modifiers/UITransitionModifier.h"

#include "Core/Log/Logger.h"
#include "Core/Maths/Maths.h"
#include "UI/UICanvas.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UITransition);

    namespace
    {
        const UIPose REST_POSE{};
    }

    // =============================================================================
    // The values in use
    // =============================================================================

    const UITransition* UITransitionModifier::GetPreset() const
    {
        if (Preset.IsEmpty() || GetOwner() == nullptr || GetOwner()->GetCanvas() == nullptr)
        {
            return nullptr;
        }

        IUIAssetProvider* lAssets = GetOwner()->GetCanvas()->GetAssets();
        if (lAssets == nullptr)
        {
            return nullptr;   // not drawn yet: no way to load it
        }

        // Read again each time: a saved preset reloads in place.
        const UITransition* lPreset = Cast<UITransition>(lAssets->ResolveAsset(Preset.Path.CStr()));
        if (lPreset == nullptr && m_WarnedPreset != Preset.Path)
        {
            m_WarnedPreset = Preset.Path;
            OPAAX_LOG(LogUITransition, Warn, "Transition preset '{}' did not load as a UI Transition — '{}' uses "
                      "its own values", Preset.Path.CStr(),
                      GetOwner()->Name.IsEmpty() ? "a widget" : GetOwner()->Name.CStr());
        }
        return lPreset;
    }

    const UIPose& UITransitionModifier::PoseFor(const EUIInteractionState InState) const
    {
        if (const UITransition* lPreset = GetPreset())
        {
            return lPreset->PoseFor(InState);
        }

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

    float UITransitionModifier::GetDuration() const
    {
        const UITransition* lPreset = GetPreset();
        return lPreset != nullptr ? lPreset->Duration : Duration;
    }

    EEase UITransitionModifier::GetEase() const
    {
        const UITransition* lPreset = GetPreset();
        return lPreset != nullptr ? lPreset->Ease : Ease;
    }

    UIPose UITransitionModifier::ShownPose() const
    {
        return IsAwake() ? m_Shown : PoseFor(m_State);
    }

    // =============================================================================
    // UIPoseModifier
    // =============================================================================

    bool UITransitionModifier::GetPose(UIPose& OutPose) const
    {
        // At rest, its state's pose as it is now: an edited or saved preset shows at the next compose.
        OutPose = ShownPose();
        return !OutPose.IsRest();
    }

    void UITransitionModifier::PostEditChange()
    {
        m_WarnedPreset.Clear();
        UIPoseModifier::PostEditChange();
    }

    // =============================================================================
    // UIModifier
    // =============================================================================

    void UITransitionModifier::OnAttach()
    {
        // Joins at its owner's state, without a motion (a button loaded disabled shows Disabled).
        m_State = GetOwner() != nullptr ? GetOwner()->GetInteractionState() : EUIInteractionState::Normal;
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
        m_From    = ShownPose();
        m_Shown   = m_From;
        m_State   = InTo;
        m_Elapsed = 0.f;

        if (GetDuration() <= 0.f)
        {
            InvalidatePose();   // its state's pose, at once
            return;
        }
        WakeUp();
    }

    bool UITransitionModifier::Tick(const float InSeconds)
    {
        m_Elapsed += InSeconds;

        const float lDuration = GetDuration();
        const float lTime     = lDuration > 0.f ? Maths::Min(m_Elapsed / lDuration, 1.f) : 1.f;
        const bool  bPlaying  = lTime < 1.f;

        if (bPlaying)
        {
            m_Shown = UIPose::Lerp(m_From, PoseFor(m_State), Easing::Evaluate(GetEase(), lTime));
        }
        InvalidatePose();

        return bPlaying;
    }
}
