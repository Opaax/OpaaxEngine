#pragma once

#include "Core/EngineAPI.h"
#include "Movement/Assets/MoveMode.h"

namespace Opaax
{
    // =============================================================================
    // GroundMoveMode — walking, jumping and falling (platformer).
    //   Quake-style: friction on the ground, acceleration with less control in the air, jump only
    //   when grounded, and gravity. MoveCapsule handles slopes, steps and walls.
    // =============================================================================
    class GroundMoveMode final : public MoveMode
    {
        OPAAX_OBJECT(GroundMoveMode, MoveMode)

        // =============================================================================
        // Tuning
        // =============================================================================
    public:
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

        OPAAX_PROPERTIES(GroundMoveMode,
                         OPAAX_PROP(GravityScale).SetRange(-5.f, 5.f),
                         OPAAX_PROP(MaxSpeed).SetRange(0.f, 5000.f),
                         OPAAX_PROP(Acceleration).SetRange(0.f, 100.f),
                         OPAAX_PROP(GroundDeceleration).SetRange(0.f, 100.f),
                         OPAAX_PROP(StopSpeed).SetRange(0.f, 1000.f),
                         OPAAX_PROP(MinSpeed).SetRange(0.f, 500.f),
                         OPAAX_PROP(AirSteer).SetRange(0.f, 1.f),
                         OPAAX_PROP(JumpSpeed).SetRange(0.f, 3000.f))

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin MoveMode interface
        void Tick(MoverTickContext& InContext) const override;
        //~End MoveMode interface
    };
}
