#include "UI/Modifiers/UITweenModifier.h"

#include "Core/Log/Logger.h"
#include "Core/Maths/Maths.h"
#include "UI/UICanvas.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UITween);

    // =============================================================================
    // Playing
    // =============================================================================

    void UITweenModifier::Play()
    {
        m_Time     = 0.f;
        m_bPlaying = true;

        // The first key's pose from now: a pop-in does not flash at full size before its first tick.
        bool lFinished = false;
        m_Shown = Sample(lFinished);

        InvalidatePose();
        WakeUp();
    }

    void UITweenModifier::Stop()
    {
        m_bPlaying = false;
        m_Shown    = UIPose{};
        InvalidatePose();
        Sleep();
        EndHide();   // a hide it was playing for ends now
    }

    void UITweenModifier::EndHide()
    {
        if (m_bForHide)
        {
            m_bForHide = false;
            FinishHide();
        }
    }

    const UITween* UITweenModifier::GetTween() const
    {
        if (Tween.IsEmpty() || GetOwner() == nullptr || GetOwner()->GetCanvas() == nullptr)
        {
            return nullptr;
        }

        IUIAssetProvider* lAssets = GetOwner()->GetCanvas()->GetAssets();
        if (lAssets == nullptr)
        {
            return nullptr;
        }

        // Read again each time: a saved tween reloads in place.
        const UITween* lTween = Cast<UITween>(lAssets->ResolveAsset(Tween.Path.CStr()));
        if (lTween == nullptr && m_WarnedTween != Tween.Path)
        {
            m_WarnedTween = Tween.Path;
            OPAAX_LOG(LogUITween, Warn, "Tween '{}' did not load as a UI Tween — '{}' plays its own keys",
                      Tween.Path.CStr(), GetOwner()->Name.IsEmpty() ? "a widget" : GetOwner()->Name.CStr());
        }
        return lTween;
    }

    UIPose UITweenModifier::Sample(bool& OutFinished) const
    {
        // Before Delay: the first key's pose.
        const float lTime = Maths::Max(m_Time - Delay, 0.f);

        if (const UITween* lTween = GetTween())
        {
            return SampleTween(*lTween, lTime, OutFinished);
        }
        return SampleTweenKeys(Keys, Loop, bHoldEnd, lTime, OutFinished);
    }

    // =============================================================================
    // Setters
    // =============================================================================

    UITweenModifier& UITweenModifier::SetTween(const OpaaxString& InAssetPath)
    {
        Tween.Path = InAssetPath;
        m_WarnedTween.Clear();
        return *this;
    }

    UITweenModifier& UITweenModifier::SetPlayOn(const EUITweenPlay InPlayOn)
    {
        PlayOn = InPlayOn;
        return *this;
    }

    UITweenModifier& UITweenModifier::SetDelay(const float InSeconds)
    {
        Delay = Maths::Max(InSeconds, 0.f);
        return *this;
    }

    // =============================================================================
    // UIPoseModifier
    // =============================================================================

    bool UITweenModifier::GetPose(UIPose& OutPose) const
    {
        OutPose = m_Shown;
        return !m_Shown.IsRest();
    }

    void UITweenModifier::PostEditChange()
    {
        SortTweenKeys(Keys);
        Delay = Maths::Max(Delay, 0.f);
        m_WarnedTween.Clear();
        UIPoseModifier::PostEditChange();
    }

    // =============================================================================
    // UIModifier
    // =============================================================================

    void UITweenModifier::OnDetach()
    {
        EndHide();   // removed while its widget waits for it: the hide does not wait for ever
        UIPoseModifier::OnDetach();
    }

    void UITweenModifier::OnReset()
    {
        // At rest; a hide it played for is over (its widget's hide is cleared with it).
        m_Time     = 0.f;
        m_bPlaying = false;
        m_bForHide = false;
        m_Shown    = UIPose{};
        InvalidatePose();
    }

    void UITweenModifier::OnShow()
    {
        if (PlayOn == EUITweenPlay::Show)
        {
            Play();
        }
        else if (PlayOn == EUITweenPlay::Hide)
        {
            Stop();   // shown again: the pose a hide held (faded out) goes
        }
    }

    bool UITweenModifier::OnHide()
    {
        if (PlayOn != EUITweenPlay::Hide)
        {
            return false;
        }

        Play();
        m_bForHide = true;
        return true;
    }

    bool UITweenModifier::Tick(const float InSeconds)
    {
        if (!m_bPlaying)
        {
            return false;
        }

        m_Time += InSeconds;

        bool lFinished = false;
        m_Shown = Sample(lFinished);
        InvalidatePose();

        if (lFinished)
        {
            m_bPlaying = false;
            EndHide();   // hidden before OnFinished: a listener sees the widget gone
            OnFinished.Broadcast();
            return false;
        }
        return true;
    }
}
