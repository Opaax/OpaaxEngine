#pragma once

#include "Core/Events/Delegate.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Resources/Asset/AssetRef.h"
#include "UI/Animation/UITween.h"   // complete: a modifier class visits its tween reference
#include "UI/UIPose.h"

namespace Opaax
{
    /**
     * When a tween modifier plays by itself. The state ones play when the widget (or the button it
     * follows) enters that state; when the state ends, a Once tween plays to its end and a repeating one
     * finishes its cycle and rests.
     */
    enum class EUITweenPlay : Uint8
    {
        Manual,   // only when asked (Play, ui.tween)
        Show,     // when its widget is shown (it joins the game's canvas shown, or Show())
        Hide,     // when its widget is hidden with Hide(): the widget hides once it is done (a loop never is)
        Hovered,  // a pulse while hovered
        Pressed,  // a bounce on press
        Focused,
        Disabled
    };

    inline const char* ToString(const EUITweenPlay InPlay) noexcept
    {
        switch (InPlay)
        {
        case EUITweenPlay::Manual:   return "Manual";
        case EUITweenPlay::Show:     return "Show";
        case EUITweenPlay::Hide:     return "Hide";
        case EUITweenPlay::Hovered:  return "Hovered";
        case EUITweenPlay::Pressed:  return "Pressed";
        case EUITweenPlay::Focused:  return "Focused";
        case EUITweenPlay::Disabled: return "Disabled";
        }
        return "Manual";
    }

    /** The interaction state InPlay plays on, when it is one of the state values. */
    inline bool TweenPlaysOnState(const EUITweenPlay InPlay, EUIInteractionState& OutState) noexcept
    {
        switch (InPlay)
        {
        case EUITweenPlay::Hovered:  OutState = EUIInteractionState::Hovered;  return true;
        case EUITweenPlay::Pressed:  OutState = EUIInteractionState::Pressed;  return true;
        case EUITweenPlay::Focused:  OutState = EUIInteractionState::Focused;  return true;
        case EUITweenPlay::Disabled: OutState = EUIInteractionState::Disabled; return true;
        default:                     return false;
        }
    }
}

OPAAX_ENUM_VALUES(Opaax::EUITweenPlay, Manual, Show, Hide, Hovered, Pressed, Focused, Disabled)

namespace Opaax
{
    DECLARE_MULTICAST_DELEGATE(FUITweenDelegate);

    // =============================================================================
    // UITweenModifier — plays keyed poses on its widget (a pop-in, a fade, a pulse, a shake): a Tween
    //   asset, or its own keys when it names none. Sampled from the time since Play, so it gives the
    //   same pose at the same moment whatever the frame rate. Delay holds the first key's pose before
    //   it starts, so a list of rows sharing one preset can be staggered. Its hit area follows by default.
    // =============================================================================
    class UITweenModifier final : public UIPoseModifier
    {
        OPAAX_OBJECT(UITweenModifier, UIPoseModifier)

    public:
        /** A UI Tween asset. When set and loaded, used instead of the keys below. */
        TAssetRef<UITween>    Tween;
        TDynArray<UITweenKey> Keys;
        EUILoop               Loop     = EUILoop::Once;
        bool                  bHoldEnd = true;
        EUITweenPlay          PlayOn   = EUITweenPlay::Manual;
        /** Seconds the first key's pose holds after Play, before it moves. */
        float                 Delay    = 0.f;

        OPAAX_PROPERTIES(UITweenModifier,
                         OPAAX_PROP(Tween).SetTooltip("A UI Tween asset. When set, its keys, loop and hold\n"
                                                      "are used instead of the ones below."),
                         OPAAX_PROP(Keys).SetTooltip("The poses and when, used without a Tween asset."),
                         OPAAX_PROP(Loop).SetTooltip("Once stops at the end, Loop starts again,\n"
                                                     "PingPong plays back and forth."),
                         OPAAX_PROP(bHoldEnd).SetTooltip("Once: the last pose stays at the end.\n"
                                                         "Off, the widget goes back to rest."),
                         OPAAX_PROP(PlayOn).SetTooltip("Manual: only when asked. Show: when the widget is shown.\n"
                                                       "Hide: when it is hidden, which waits for it.\n"
                                                       "Hovered, Pressed, Focused, Disabled: when the widget (or the button\n"
                                                       "it follows) enters that state; when it leaves, a loop finishes\n"
                                                       "its cycle and rests, a Once tween plays to its end."),
                         OPAAX_PROP(Delay).SetRange(0.f, 60.f).SetDragStep(0.01f)
                                          .SetTooltip("Seconds the first key's pose holds before it moves.\n"
                                                      "Stagger a list sharing one tween."))

        /** Plays from the start (again, if it was playing). */
        void Play();

        /** Stops and puts the widget at rest. */
        void Stop();

        /** Whether it moves now (a delay included). A finished Once tween is not playing. */
        bool IsPlaying() const noexcept { return m_bPlaying; }

        /** A state tween whose state ended: it plays on to the end of its cycle, then rests. */
        bool IsEnding() const noexcept { return m_bEnding; }

        /** Seconds since Play, the delay included. */
        float GetTime() const noexcept { return m_Time; }

        /** The tween in use: Tween's asset through the owner's canvas, or null (then its own keys). */
        const UITween* GetTween() const;

        /** Fired once when a Once tween reaches its end (never for a loop, nor when stopped). */
        FUITweenDelegate OnFinished;

        // Setters that chain: lWidget->AddModifier<UITweenModifier>().SetTween(...).SetPlayOn(...)
        UITweenModifier& SetTween(const OpaaxString& InAssetPath);
        UITweenModifier& SetPlayOn(EUITweenPlay InPlayOn);
        UITweenModifier& SetDelay(float InSeconds);

        //~Begin UIPoseModifier interface
        bool GetPose(UIPose& OutPose) const override;
        void PostEditChange() override;
        //~End UIPoseModifier interface

    protected:
        //~Begin UIModifier interface
        void OnDetach() override;
        void OnReset() override;
        void OnShow() override;
        bool OnHide() override;
        bool Tick(float InSeconds) override;
        void OnStateChanged(EUIInteractionState InFrom, EUIInteractionState InTo) override;
        //~End UIModifier interface

    private:
        /** The pose at m_Time, and whether a Once tween is past its end. */
        UIPose Sample(bool& OutFinished) const;

        /** Seconds one cycle of the keys in use takes when they repeat (0 for Once). */
        float GetCycleLength() const;

        /** Lets its owner's hide finish (at its end, or when stopped). */
        void EndHide();

        float  m_Time     = 0.f;
        bool   m_bPlaying = false;
        /** Playing for a Hide(): the widget hides when it ends. */
        bool   m_bForHide = false;
        /** Its state ended: it rests once its time (past Delay) reaches m_EndAt. */
        bool   m_bEnding  = false;
        float  m_EndAt    = 0.f;
        /** What it shows: while playing, at the end it holds, or the rest pose. */
        UIPose m_Shown;

        /** The tween path already warned about, so a missing one is said once. */
        mutable OpaaxString m_WarnedTween;
    };
}
