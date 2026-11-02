#pragma once

#include "Core/Maths/Bounds2D.h"
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
    // CameraFollowSettings — how a camera follows a target.
    // =============================================================================
    struct CameraFollowSettings
    {
        /** Where the camera aims, from the target. */
        Vector2F Offset = { 0.f, 0.f };

        /** The part of the view the aim moves in without the camera, as a fraction of its width and height. */
        Vector2F DeadZone = { 0.f, 0.f };

        /** Seconds the camera takes to settle on the aim, per axis (SmoothDamp's time); 0 keeps it on it. */
        Vector2F Damping = { 0.f, 0.f };

        /** Seconds of the target's velocity the aim leads it by. */
        float LookAhead = 0.f;

        /** Seconds the lead takes to settle when the target's velocity changes; 0 at once. */
        float LookAheadDamping = 0.f;
    };

    // =============================================================================
    // CameraFollowState — what a follow carries from one step to the next.
    // =============================================================================
    struct CameraFollowState
    {
        /** The camera's velocity, per axis, as SmoothDamp carries it. */
        Vector2F Velocity = { 0.f, 0.f };

        /** How far the aim leads the target, and its velocity. */
        Vector2F Lead         = { 0.f, 0.f };
        Vector2F LeadVelocity = { 0.f, 0.f };

        /** Where the last step saw the target: its velocity is measured over the steps. */
        Vector2F LastTarget = { 0.f, 0.f };
        bool     bHasTarget = false;

        /** The last step held the camera: the aim was inside the dead zone both ways. */
        bool bInDeadZone = false;
    };

    // =============================================================================
    // Follow (pure function)
    // =============================================================================
    /**
     * Where a camera at InCamera is after InDeltaTime following a target at InTarget. It aims at the
     * target plus the offset plus a lead (its velocity over the last step times LookAhead); while the
     * aim is inside the dead zone the camera holds, outside it moves by the overshoot, damped per axis.
     * A step of InDeltaTime gives what two of half of it give, the target still.
     * @param InViewHalfExtent Half the view's width and height, in world units
     */
    inline Vector2F StepCameraFollow(const Vector2F InCamera, const Vector2F InTarget, const Vector2F InViewHalfExtent,
                                     const CameraFollowSettings& InSettings, CameraFollowState& InOutState,
                                     const float InDeltaTime)
    {
        // Measured over the step, so it is the same at any frame rate; none on the first.
        Vector2F lTargetVelocity{ 0.f, 0.f };
        if (InOutState.bHasTarget && InDeltaTime > 0.f)
        {
            lTargetVelocity = (InTarget - InOutState.LastTarget) / InDeltaTime;
        }
        InOutState.LastTarget = InTarget;
        InOutState.bHasTarget = true;

        Vector2F lCamera       = InCamera;
        InOutState.bInDeadZone = true;
        for (Int32 lAxis = 0; lAxis < 2; ++lAxis)
        {
            InOutState.Lead[lAxis] = DampCameraAxis(InOutState.Lead[lAxis], lTargetVelocity[lAxis] * InSettings.LookAhead,
                                                    InOutState.LeadVelocity[lAxis], InSettings.LookAheadDamping,
                                                    InDeltaTime);

            const float lAim  = InTarget[lAxis] + InSettings.Offset[lAxis] + InOutState.Lead[lAxis];
            const float lHalf = Maths::Max(InSettings.DeadZone[lAxis], 0.f) * InViewHalfExtent[lAxis];
            const float lGap  = lAim - InCamera[lAxis];

            float lGoal = InCamera[lAxis];
            if (lGap > lHalf)
            {
                lGoal                  = lAim - lHalf;
                InOutState.bInDeadZone = false;
            }
            else if (lGap < -lHalf)
            {
                lGoal                  = lAim + lHalf;
                InOutState.bInDeadZone = false;
            }
            lCamera[lAxis] = DampCameraAxis(InCamera[lAxis], lGoal, InOutState.Velocity[lAxis],
                                            InSettings.Damping[lAxis], InDeltaTime);
        }
        return lCamera;
    }

    /** Where the aim moves without a camera at InCamera: InDeadZone of a view InViewHalfExtent each side. */
    inline Bounds2D CameraDeadZoneBounds(const Vector2F InCamera, const Vector2F InDeadZone, const Vector2F InViewHalfExtent)
    {
        return Bounds2D{ InCamera, { Maths::Max(InDeadZone.x, 0.f) * InViewHalfExtent.x,
                                     Maths::Max(InDeadZone.y, 0.f) * InViewHalfExtent.y } };
    }

    // =============================================================================
    // CameraFollowStage — a Body stage that follows an entity (StepCameraFollow): a dead zone the
    //   target moves in freely, damping per axis, and a look-ahead along its velocity. The view's
    //   width counts the screen's aspect (World::GetViewAspect).
    // =============================================================================
    class CameraFollowStage final : public CameraStage
    {
        OPAAX_OBJECT(CameraFollowStage, CameraStage)

    public:
        /** The entity it follows, by name: found again once it is gone (a respawned player). */
        OpaaxString Target;

        Vector2F Offset           = { 0.f, 0.f };
        Vector2F DeadZone         = { 0.f, 0.f };
        Vector2F Damping          = { 0.f, 0.f };
        float    LookAhead        = 0.f;
        float    LookAheadDamping = 0.f;

        OPAAX_PROPERTIES(CameraFollowStage,
                         OPAAX_PROP(Target).SetTooltip("The entity it follows, by name."),
                         OPAAX_PROP(Offset).SetTooltip("Where it aims, from the target."),
                         OPAAX_PROP(DeadZone).SetTooltip("The part of the view the aim moves in without the camera, "
                                                         "as a fraction of its width and height.\n0: on the aim."),
                         OPAAX_PROP(Damping).SetTooltip("Seconds the camera takes to settle on the aim, per axis.\n"
                                                        "0: on it at once."),
                         OPAAX_PROP(LookAhead).SetRange(0.f, 5.f).SetTooltip("Seconds of the target's velocity the "
                                                                             "aim leads it by."),
                         OPAAX_PROP(LookAheadDamping).SetRange(0.f, 5.f).SetTooltip("Seconds the lead takes to settle "
                                                                                    "when the target's velocity "
                                                                                    "changes.\n0: at once."))

        /** The settings above, as StepCameraFollow takes them. */
        CameraFollowSettings GetSettings() const noexcept
        {
            return CameraFollowSettings{ Offset, DeadZone, Damping, LookAhead, LookAheadDamping };
        }

        /** What it carries from step to step. */
        const CameraFollowState& GetState() const noexcept { return m_State; }

        //~Begin CameraStage interface
        ECameraStage GetStage() const override { return ECameraStage::Body; }
        void         Step(CameraRigPose& InOutPose, float InDeltaTime) override;
        //~End CameraStage interface

    private:
        Entity            m_Target;
        CameraFollowState m_State;
    };
}
