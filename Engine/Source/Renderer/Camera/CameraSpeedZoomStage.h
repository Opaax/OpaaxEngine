#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Object/ObjectMacros.h"
#include "Renderer/Camera/CameraStage.h"
#include "World/Entity/Entity.h"

namespace Opaax
{
    // =============================================================================
    // CameraSpeedZoomSettings — how a camera zooms out with a target's speed.
    // =============================================================================
    struct CameraSpeedZoomSettings
    {
        /** The OrthoSize at rest. */
        float BaseSize = 300.f;

        /** The OrthoSize added at FullSpeed and above. */
        float ZoomOut = 0.f;

        /** World units a second at which the whole ZoomOut is reached. */
        float FullSpeed = 1000.f;

        /** Seconds to close half the gap to the size the speed asks for; 0 at once. */
        float HalfLife = 0.35f;
    };

    // =============================================================================
    // CameraSpeedZoomState — what a speed zoom carries from one step to the next.
    // =============================================================================
    struct CameraSpeedZoomState
    {
        float Size = 0.f;

        /** Where the last step saw the target: its speed is measured over the steps. */
        Vector2F LastTarget = { 0.f, 0.f };
        bool     bHasTarget = false;
    };

    // =============================================================================
    // Speed zoom (pure functions)
    // =============================================================================
    /** The OrthoSize InSettings ask for at InSpeed: BaseSize, plus ZoomOut in proportion up to FullSpeed. */
    inline float CameraSpeedZoomGoal(const CameraSpeedZoomSettings& InSettings, const float InSpeed)
    {
        const float lFraction = InSettings.FullSpeed > 0.f ? Maths::Clamp(InSpeed / InSettings.FullSpeed, 0.f, 1.f) : 1.f;
        return InSettings.BaseSize + InSettings.ZoomOut * lFraction;
    }

    /**
     * The OrthoSize after InDeltaTime with a target at InTarget: its speed measured over the step (none
     * on the first), the size eased toward the goal by ExpDecay, half the gap closed every HalfLife.
     * A step of InDeltaTime gives what two of half of it give, the speed steady.
     */
    inline float StepCameraSpeedZoom(const Vector2F InTarget, const CameraSpeedZoomSettings& InSettings,
                                     CameraSpeedZoomState& InOutState, const float InDeltaTime)
    {
        float lSpeed = 0.f;
        if (InOutState.bHasTarget && InDeltaTime > 0.f)
        {
            const Vector2F lMoved = InTarget - InOutState.LastTarget;
            lSpeed                = Maths::Sqrt(lMoved.x * lMoved.x + lMoved.y * lMoved.y) / InDeltaTime;
        }
        InOutState.LastTarget = InTarget;
        InOutState.bHasTarget = true;

        // A decay of ln 2 / HalfLife halves the gap every HalfLife.
        constexpr float LN2   = 0.693147181f;
        const float     lGoal = CameraSpeedZoomGoal(InSettings, lSpeed);
        InOutState.Size       = InSettings.HalfLife > 0.f
                                    ? Maths::ExpDecay(InOutState.Size, lGoal, LN2 / InSettings.HalfLife, InDeltaTime)
                                    : lGoal;
        return InOutState.Size;
    }

    // =============================================================================
    // CameraSpeedZoomStage — a Size stage that zooms out with an entity's speed
    //   (StepCameraSpeedZoom), as a racing or momentum game's camera does: BaseSize at rest (0: the
    //   camera's size when it starts), ZoomOut more at FullSpeed, eased by HalfLife.
    // =============================================================================
    class CameraSpeedZoomStage final : public CameraStage
    {
        OPAAX_OBJECT(CameraSpeedZoomStage, CameraStage)

    public:
        /** The entity whose speed zooms the camera, by name: found again once it is gone. */
        OpaaxString Target;

        float BaseSize  = 0.f;
        float ZoomOut   = 200.f;
        float FullSpeed = 1000.f;
        float HalfLife  = 0.35f;

        OPAAX_PROPERTIES(CameraSpeedZoomStage,
                         OPAAX_PROP(Target).SetTooltip("The entity whose speed zooms the camera, by name."),
                         OPAAX_PROP(BaseSize).SetRange(0.f, 100000.f).SetTooltip("The OrthoSize at rest.\n"
                                                                                 "0: the camera's when it starts."),
                         OPAAX_PROP(ZoomOut).SetRange(0.f, 100000.f).SetTooltip("The OrthoSize added at FullSpeed "
                                                                                "and above."),
                         OPAAX_PROP(FullSpeed).SetRange(0.f, 100000.f).SetTooltip("Units a second at which the "
                                                                                  "whole ZoomOut is reached."),
                         OPAAX_PROP(HalfLife).SetRange(0.f, 10.f).SetTooltip("Seconds to close half the gap to "
                                                                             "the size the speed asks for.\n"
                                                                             "0: at once."))

        /** What it carries from step to step. */
        const CameraSpeedZoomState& GetState() const noexcept { return m_State; }

        //~Begin CameraStage interface
        ECameraStage GetStage() const override { return ECameraStage::Size; }
        void         Step(CameraRigPose& InOutPose, float InDeltaTime) override;
        //~End CameraStage interface

    private:
        Entity               m_Target;
        CameraSpeedZoomState m_State;
        float                m_StartSize = 0.f;
        bool                 m_bStarted  = false;
    };
}
