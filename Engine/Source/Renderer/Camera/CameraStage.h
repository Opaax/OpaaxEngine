#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Object/ObjectMacros.h"
#include "World/Behaviour/Behaviour.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A camera stage is a part of a camera rig: a behaviour on a camera that places, zooms or bounds it.
// The world's CameraRigSubsystem runs a camera's stages in each fixed step, its Body stages first,
// then its Size stages, then its Constraint stages, each on the pose the ones before it left, and
// writes the result to the camera once. A rigged camera so moves in the steps only, in one place,
// and is drawn between its steps at any frame rate:
//
//   class DriftStage final : public Opaax::CameraStage
//   {
//       OPAAX_OBJECT(DriftStage, Opaax::CameraStage)
//
//   public:
//       float Speed = 40.f;
//       OPAAX_PROPERTIES(DriftStage, OPAAX_PROP(Speed))
//
//       Opaax::ECameraStage GetStage() const override { return Opaax::ECameraStage::Body; }
//
//       void Step(Opaax::CameraRigPose& InOutPose, const float InDeltaTime) override
//       {
//           InOutPose.Position.x += Speed * InDeltaTime;
//       }
//   };
//   OPAAX_REGISTER_BEHAVIOUR(DriftStage);
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    /** When a stage runs among its camera's stages: places it, then zooms it, then bounds it. */
    enum class ECameraStage : Uint8
    {
        Body,
        Size,
        Constraint
    };

    constexpr const char* ToString(const ECameraStage InStage) noexcept
    {
        switch (InStage)
        {
            case ECameraStage::Body:       return "Body";
            case ECameraStage::Size:       return "Size";
            case ECameraStage::Constraint: return "Constraint";
        }
        return "Unknown";
    }

    // =============================================================================
    // CameraRigPose — what a camera's stages work on in a step: its pose in the world and its size.
    // =============================================================================
    struct CameraRigPose
    {
        Vector2F Position = { 0.f, 0.f };

        /** Degrees, as the camera's Transform. */
        float Rotation = 0.f;

        /** The camera's OrthoSize: half the height it frames. */
        float OrthoSize = 300.f;
    };

    /**
     * InCurrent moved toward InGoal over InDeltaTime as SmoothDamp moves it in InTime seconds, on it at
     * once with 0: how stages ease, the same over one step or two halves of it.
     */
    inline float DampCameraAxis(const float InCurrent, const float InGoal, float& InOutVelocity, const float InTime,
                                const float InDeltaTime)
    {
        if (InTime <= 0.f)
        {
            InOutVelocity = 0.f;
            return InGoal;
        }
        return Maths::SmoothDamp(InCurrent, InGoal, InOutVelocity, InTime, InDeltaTime);
    }

    // =============================================================================
    // CameraStage — a behaviour on a camera that changes its pose in the fixed steps, run by the
    //   world's CameraRigSubsystem. See the usage notes above.
    // =============================================================================
    class CameraStage : public Behaviour
    {
        OPAAX_ABSTRACT_OBJECT(CameraStage, Behaviour)

    protected:
        CameraStage() = default;

    public:
        /** When it runs among the camera's stages. */
        virtual ECameraStage GetStage() const = 0;

        /** Changes InOutPose, the camera's pose as the stages before it left it this step. */
        virtual void Step(CameraRigPose& InOutPose, float InDeltaTime) = 0;
    };
}
