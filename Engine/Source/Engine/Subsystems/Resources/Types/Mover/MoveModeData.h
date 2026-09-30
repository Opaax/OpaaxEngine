#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/Maths.h"   // Cos, DegreesToRadians
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"

namespace Opaax
{
    // =============================================================================
    // MoveModeData — one movement tuning (.opaaxmovemode), shared by every entity that moves this way.
    //   Mode names the behaviour (a registered IMoverMode); the other fields are its settings.
    //   Each mode reads the fields it needs; the editor shows only those.
    // =============================================================================
    struct MoveModeData
    {
        /**
         * The registered IMoverMode to use. An unknown id means the mover does not move.
         */
        OpaaxStringID Mode = OPAAX_ID("GroundMove");

        /** Multiplies the world's gravity vector, direction included. 0 floats, 2 is heavy. */
        float GravityScale = 1.f;

        /** Target speed at full input, world units/s. */
        float MaxSpeed = 400.f;

        /** How fast MaxSpeed is reached. Higher is snappier. */
        float Acceleration = 10.f;

        /** Friction rate (1/s) on the ground. */
        float GroundDeceleration = 8.f;

        /** Below this speed friction applies at a fixed rate instead of proportionally. */
        float StopSpeed = 100.f;

        /** Below this speed the mover stops. */
        float MinSpeed = 10.f;

        /** [0..1] fraction of ground acceleration available in the air. */
        float AirSteer = 0.3f;

        /** Upward velocity of a jump. Grounded only. */
        float JumpSpeed = 500.f;

        /** Steepest slope that still counts as ground, in degrees. */
        float MaxSlopeAngleDeg = 50.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MoveModeData, Mode, GravityScale, MaxSpeed,
                                                    Acceleration, GroundDeceleration, StopSpeed,
                                                    MinSpeed, AirSteer, JumpSpeed, MaxSlopeAngleDeg)

        OPAAX_PROPERTIES(MoveModeData,
                         OPAAX_PROP(Mode).SetTooltip("Which registered mover mode reads this tuning."),
                         OPAAX_PROP(GravityScale).SetRange(-5.f, 5.f),
                         OPAAX_PROP(MaxSpeed).SetRange(0.f, 5000.f),
                         OPAAX_PROP(Acceleration).SetRange(0.f, 100.f),
                         OPAAX_PROP(GroundDeceleration).SetRange(0.f, 100.f),
                         OPAAX_PROP(StopSpeed).SetRange(0.f, 1000.f),
                         OPAAX_PROP(MinSpeed).SetRange(0.f, 500.f),
                         OPAAX_PROP(AirSteer).SetRange(0.f, 1.f),
                         OPAAX_PROP(JumpSpeed).SetRange(0.f, 3000.f),
                         OPAAX_PROP(MaxSlopeAngleDeg).SetRange(0.f, 89.f))

        /**
         * cos(MaxSlopeAngleDeg): the minimum surface normal Y that counts as ground.
         */
        float GroundNormalY() const noexcept
        {
            return Maths::Cos(Maths::DegreesToRadians(MaxSlopeAngleDeg));
        }
    };
}
