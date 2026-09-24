#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/Maths.h"   // Cos + DegreesToRadians — GroundNormalY is inline (I6)
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"

namespace Opaax
{
    // =============================================================================
    // MoveModeData — one movement tuning. The `.opaaxmovemode` payload, and the CLIP of the
    //   mover family: authored once, shared by every entity that moves this way, and the unit
    //   that grows when a mode needs a new knob.
    //
    //   `Mode` NAMES THE BEHAVIOUR, the rest is its knobs. That split is what keeps this ONE
    //   resource type instead of one per mode: a clip is one type too, and clips differ in DATA.
    //   The registered IMoverMode with that id reads the fields it uses and ignores the others —
    //   FlyMove reads MaxSpeed alone, which is the whole of what it ever needed.
    //
    //   THE UNUSED FIELDS ARE AN EDITOR PROBLEM, NOT AN ENGINE ONE. The document panel knows
    //   which mode is selected and shows only that mode's knobs; the engine never has to care,
    //   and a mode a GAME registers gets the same set with no engine change.
    // =============================================================================
    struct MoveModeData
    {
        /**
         * Which registered IMoverMode drives this tuning. An id nothing registered resolves to no
         * mode, and the mover does not move — a visible, nameable state rather than a silent one.
         */
        OpaaxStringID Mode = OPAAX_ID("GroundMove");

        /** Multiplies the world's gravity vector, direction included. 0 floats, 2 is heavy. */
        float GravityScale = 1.f;

        /** Target speed at full directional input, world units/s. Every mode reads this one. */
        float MaxSpeed = 400.f;

        /** Responsiveness toward MaxSpeed. Higher turns on a coin, lower drifts. */
        float Acceleration = 10.f;

        /** Friction RATE (1/s) while grounded — not a 0..1 surface coefficient. */
        float GroundDeceleration = 8.f;

        /** Below this speed friction applies at a fixed rate instead of proportionally. */
        float StopSpeed = 100.f;

        /** Below this the mover snaps to rest, so it never creeps at a pixel a second. */
        float MinSpeed = 10.f;

        /** [0..1] fraction of ground acceleration available in the air. */
        float AirSteer = 0.3f;

        /** Upward velocity set by a jump. Only a grounded mover may spend it. */
        float JumpSpeed = 500.f;

        /** Steepest slope that still counts as ground, degrees. */
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
         * cos(MaxSlopeAngleDeg) — the minimum surface-normal Y the sweep counts as ground.
         *
         * INLINE, like every member here: the struct carries no OPAAX_API, so anything defined in
         * the DLL's .cpp would be unresolvable from the exe (**I6**).
         */
        float GroundNormalY() const noexcept
        {
            return Maths::Cos(Maths::DegreesToRadians(MaxSlopeAngleDeg));
        }
    };
}
