#pragma once

#include "Core/Maths/Easing.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "UI/UIPose.h"

namespace Opaax
{
    // =============================================================================
    // UITransitionModifier — a pose per interaction state, eased to over Duration when the state
    //   changes (CSS transitions, Unity's Selectable transitions). Normal is the rest pose. An
    //   interrupted transition starts again from the pose it shows: no jump. Its hit area stays the
    //   layout's by default, so a press that shrinks its button cannot cancel itself.
    // =============================================================================
    class UITransitionModifier final : public UIPoseModifier
    {
        OPAAX_OBJECT(UITransitionModifier, UIPoseModifier)

    public:
        UITransitionModifier() { bMovesHitArea = false; }

        /** Seconds from one state's pose to the next. 0 jumps. */
        float  Duration = 0.12f;
        EEase  Ease     = EEase::QuadOut;
        UIPose Hovered;
        UIPose Pressed;
        UIPose Focused;
        UIPose Disabled;

        OPAAX_PROPERTIES(UITransitionModifier,
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

        /** The pose an interaction state asks for (Normal: the rest pose). */
        const UIPose& PoseFor(EUIInteractionState InState) const noexcept;

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
        EUIInteractionState m_State   = EUIInteractionState::Normal;
        UIPose              m_From;
        UIPose              m_Shown;
        float               m_Elapsed = 0.f;
    };
}
