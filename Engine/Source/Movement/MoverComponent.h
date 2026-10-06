#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"        // Max
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    struct MoverResource;

    // =============================================================================
    // MoverInput — this step's intent, written by a controller (player, AI, ...) and read by
    //   the active mode. Runtime only, not saved.
    // =============================================================================
    struct MoverInput
    {
        /** Desired direction. Ground modes use x as a [-1..1] throttle and ignore y. */
        Vector2F MoveDir = { 0.f, 0.f };

        /** One-shot request, consumed by the mode. */
        bool bJump = false;
    };

    // =============================================================================
    // MoverComponent — kinematic movement driven by an IMoverMode.
    //   Sweeps a capsule against the world and moves its Transform. Do not add a ColliderComponent too.
    //   Behaviour lives in the modes; tuning lives in the .opaaxmover / .opaaxmovemode assets.
    //   This holds the capsule (per entity) and the runtime state.
    // =============================================================================
    struct MoverComponent
    {
        // ---- modes (saved) ---------------------------------------------------------------------
        /** Asset-relative ("Movers/Hero.opaaxmover"). Empty: does not move. */
        TResourcePath<MoverResource> Mover;

        /** Active mode. Invalid means the mover's default. */
        OpaaxStringID ModeName;

        // ---- collision capsule (saved) ---------------------------------------------------------
        /** Total capsule height, world units. Below twice the radius it becomes a circle. */
        float Height = 100.f;

        /** Capsule radius, world units. */
        float Radius = 25.f;

        /** Channels the capsule collides with. */
        ECollisionChannel Channel = ECollisionChannel::Pawn;

        // ---- state (runtime, not saved) --------------------------------------------------------
        Vector2F Velocity     = { 0.f, 0.f };
        bool     bGrounded    = false;
        Vector2F GroundNormal = { 0.f, 0.f };

        // ---- intent (runtime, not saved) -------------------------------------------------------
        MoverInput Input;

        /**
         * Requested mode change, applied by MoverSubsystem next step (with OnModeExit/OnModeEnter).
         * Invalid = none.
         */
        OpaaxStringID PendingMode;

        // Runtime fields are not saved.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MoverComponent, Mover, ModeName, Height, Radius, Channel)

        OPAAX_PROPERTIES(MoverComponent,
                         OPAAX_PROP(Mover).SetTooltip("The .opaaxmover naming this thing's modes."),
                         OPAAX_PROP(ModeName).SetTooltip("Which mode to start in.\n"
                                                         "Empty uses the mover's own default."),
                         OPAAX_PROP(Height).SetRange(1.f, 2048.f)
                                           .SetTooltip("Total capsule height.\n"
                                                       "Below twice the radius it becomes a circle."),
                         OPAAX_PROP(Radius).SetRange(1.f, 1024.f),
                         OPAAX_PROP(Channel).SetTooltip("What this mover IS, for collision filtering."))

        /**
         * The capsule in local space. The caps sit a radius in from each end, so the total height is Height.
         */
        MoverCapsule BuildCapsule() const noexcept
        {
            const float lHalfSpan = Maths::Max(0.f, Height * 0.5f - Radius);

            MoverCapsule lCapsule;
            lCapsule.Center1 = { 0.f, -lHalfSpan };
            lCapsule.Center2 = { 0.f,  lHalfSpan };
            lCapsule.Radius  = Radius;
            return lCapsule;
        }
    };
}
