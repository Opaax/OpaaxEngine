#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"        // Max — BuildCapsule is inline (I6)
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
    struct MoverResource;   // only NAMED — TResourcePath never completes its parameter

    // =============================================================================
    // MoverInput — the per-step INTENT, written by a producer and read by a mode.
    //
    //   The Input / Simulation / State seam: any producer writes it — a player controller, an AI,
    //   a cutscene — and the active mode consumes it. Runtime only, never serialized: intent is
    //   what is being asked THIS step, and a saved map has nobody asking.
    // =============================================================================
    struct MoverInput
    {
        /** Desired direction. For a ground mode, x is a [-1..1] throttle and y is ignored. */
        Vector2F MoveDir = { 0.f, 0.f };

        /** An EDGE, consumed by the mode when it is spent — not a held state. */
        bool bJump = false;
    };

    // =============================================================================
    // MoverComponent — a kinematic mover: DUMB DATA plus intent, driven by an IMoverMode.
    //
    //   NOT a simulated rigid body. It sweeps a capsule against the world (collide-and-slide) and
    //   resolves its own Transform, so an entity carrying one should NOT also carry a
    //   ColliderComponent — that would build a second, simulated body in the same place.
    //
    //   BEHAVIOUR IS NOT HERE. It lives in the mode the Mover asset names, so this component never
    //   grows movement logic — the anti-CharacterMovementComponent stance, which is the whole
    //   reason the mode is a separate thing at all.
    //
    //   TUNING IS NOT HERE EITHER, and that is P5a's payoff: `Mover` names a `.opaaxmover` bag,
    //   whose entries name `.opaaxmovemode` tunings. What lives on the entity is the collision
    //   PROXY (a capsule is per-entity geometry, like a collider's size) and the runtime state.
    // =============================================================================
    struct MoverComponent
    {
        // ---- the bag of modes this thing can move in (serialized) ---------------------------
        /** Asset-relative ("Movers/Hero.opaaxmover"). EMPTY moves nothing — a real state. */
        TResourcePath<MoverResource> Mover;

        /** Which of the bag's modes is active. INVALID means "the bag's default". */
        OpaaxStringID ModeName;

        // ---- collision proxy (serialized) — per-entity geometry, not shared tuning -----------
        /** Total capsule height, world units. Below twice the radius it degenerates to a circle. */
        float Height = 100.f;

        /** Capsule radius, world units. */
        float Radius = 25.f;

        /** Which channels are SOLID to movement — what the sweep slides on. */
        ECollisionChannel Channel = ECollisionChannel::Pawn;

        // ---- sync state (runtime, NOT serialized) --------------------------------------------
        Vector2F Velocity     = { 0.f, 0.f };
        bool     bGrounded    = false;
        Vector2F GroundNormal = { 0.f, 0.f };

        // ---- intent (runtime, producer-written, NOT serialized) ------------------------------
        MoverInput Input;

        /**
         * A queued mode switch, consumed by MoverSubsystem next step. Invalid = nothing pending.
         *
         * DEFERRED rather than immediate, so a producer can ask at any point in a frame and the
         * transition still happens at a defined moment — with OnModeExit and OnModeEnter fired in
         * order, between steps rather than in the middle of one.
         */
        OpaaxStringID PendingMode;

        // Runtime fields are absent from BOTH macros on purpose: velocity and intent are what the
        // simulation is DOING, not what the map says, and a saved grounded flag would be a lie the
        // first time the map loaded somewhere else.
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
         * The local-space capsule the sweep uses. Inline for **I6**: this struct carries no
         * OPAAX_API, so a member defined in the DLL's .cpp is unresolvable from the exe.
         *
         * The caps sit a radius in from each end, so a capsule of Height is exactly that tall —
         * the same arithmetic `PhysicsSubsystem::MakeShapeDesc` does for a capsule collider.
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
