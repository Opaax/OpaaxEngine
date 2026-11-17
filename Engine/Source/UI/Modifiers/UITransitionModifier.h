#pragma once

#include "Core/Maths/Easing.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Resources/Asset/AssetRef.h"
#include "UI/Animation/UITransition.h"   // complete: a modifier class visits its preset reference
#include "UI/UIPose.h"

namespace Opaax
{
    // =============================================================================
    // UITransitionModifier — a pose per interaction state, eased to over Duration when the state
    //   changes (CSS transitions, Unity's Selectable transitions). Normal is the rest pose. An
    //   interrupted transition starts again from the pose it shows: no jump. Its hit area stays the
    //   layout's by default, so a press that shrinks its button cannot cancel itself.
    //   A Preset (a UITransition asset) wins over its own values when it names one that loads.
    // =============================================================================
    class UITransitionModifier final : public UIPoseModifier
    {
        OPAAX_OBJECT(UITransitionModifier, UIPoseModifier)

    public:
        UITransitionModifier() { bMovesHitArea = false; }

        /** Shared values: when set and loaded, used instead of the fields below. */
        TAssetRef<UITransition> Preset;
        /** Seconds from one state's pose to the next. 0 jumps. */
        float  Duration = 0.12f;
        EEase  Ease     = EEase::QuadOut;
        UIPose Hovered;
        UIPose Pressed;
        UIPose Focused;
        UIPose Disabled;

        OPAAX_PROPERTIES(UITransitionModifier,
                         OPAAX_PROP(Preset).SetTooltip("A UI Transition asset. When set, its values are used\n"
                                                       "instead of the ones below."),
                         OPAAX_PROP(Duration).SetRange(0.f, 10.f).SetDragStep(0.01f)
                                             .SetTooltip("Seconds from one state's pose to the next. 0 jumps."),
                         OPAAX_PROP(Ease).SetTooltip("The curve of the motion."),
                         OPAAX_PROP(Hovered).SetTooltip("The pose under the pointer."),
                         OPAAX_PROP(Pressed).SetTooltip("The pose while held (and still under the pointer)."),
                         OPAAX_PROP(Focused).SetTooltip("The pose while it takes the keys."),
                         OPAAX_PROP(Disabled).SetTooltip("The pose while it takes no input (a disabled button)."))

        /** The state whose pose it shows or goes to. */
        EUIInteractionState GetState() const noexcept { return m_State; }

        /** Whether it is between two poses. */
        bool IsPlaying() const noexcept { return IsAwake(); }

        /**
         * The preset in use: Preset's asset through the owner's canvas, or null (none named, no canvas
         * drawn yet, or it did not load: then its own values, and one warning).
         */
        const UITransition* GetPreset() const;

        /** The pose an interaction state asks for (Normal: the rest pose), the preset's when it has one. */
        const UIPose& PoseFor(EUIInteractionState InState) const;

        /** The duration and the curve in use, the preset's when it has one. */
        float GetDuration() const;
        EEase GetEase() const;

        //~Begin UIPoseModifier interface
        bool GetPose(UIPose& OutPose) const override;
        void PostEditChange() override;
        //~End UIPoseModifier interface

    protected:
        //~Begin UIModifier interface
        void OnAttach() override;
        void OnStateChanged(EUIInteractionState InFrom, EUIInteractionState InTo) override;
        bool Tick(float InSeconds) override;
        //~End UIModifier interface

    private:
        /** The pose shown now: between two while playing, its state's otherwise. */
        UIPose ShownPose() const;

        EUIInteractionState m_State   = EUIInteractionState::Normal;
        UIPose              m_From;
        UIPose              m_Shown;
        float               m_Elapsed = 0.f;

        /** The preset path already warned about, so a missing one is said once. */
        mutable OpaaxString m_WarnedPreset;
    };
}
