#pragma once

#include "Core/Maths/Maths.h"   // Cos, DegreesToRadians
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/Asset.h"
#include "World/Entity/Entity.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A way of moving is a MoveMode subclass: its tuning as properties and its step as Tick. Its
// assets are the tunings a Mover names:
//
//   class SwimMoveMode final : public Opaax::MoveMode
//   {
//       OPAAX_OBJECT(SwimMoveMode, Opaax::MoveMode)
//
//   public:
//       float MaxSpeed = 200.f;
//       OPAAX_PROPERTIES(SwimMoveMode, OPAAX_PROP(MaxSpeed).SetRange(0.f, 2000.f))
//
//       void Tick(Opaax::MoverTickContext& InContext) const override;
//   };
//   OPAAX_REGISTER_OBJECT(SwimMoveMode);
//
// One asset serves every entity that moves that way: a mode keeps no state, the entity's lives on
// its MoverComponent. Hence the const hooks.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    class IPhysicsWorld;
    struct MoverComponent;
    struct TransformComponent;

    // =============================================================================
    // MoverTickContext — what a mode needs for one entity's step.
    // =============================================================================
    struct MoverTickContext
    {
        IPhysicsWorld&      World;
        MoverComponent&     Mover;
        TransformComponent& Transform;

        float DeltaTime = 0.f;

        /**
         * The mover's own body user data (so the sweep skips it). 0 on transition calls.
         */
        Uint64 SelfUserData = 0;

        /**
         * The entity being moved, so a mode can read its other components (a game mode's own state).
         * Invalid when a mode is driven outside a world.
         */
        Entity Owner;
    };

    // =============================================================================
    // MoveMode — one way of moving (walking, flying...) and its tuning, shared by every entity that
    //   moves this way (the movement modes of Unreal's Mover). Reads intent and state from the
    //   component, sweeps through MoveCapsule, writes the result. Abstract: an asset holds one of its
    //   subclasses (GroundMoveMode, FlyMoveMode, a game's own).
    // =============================================================================
    class MoveMode : public Asset
    {
        OPAAX_ABSTRACT_OBJECT(MoveMode, Asset)

        // =============================================================================
        // Tuning every mode shares
        // =============================================================================
    public:
        /** Steepest slope that still counts as ground, in degrees. */
        float MaxSlopeAngleDeg = 50.f;

        OPAAX_PROPERTIES(MoveMode, OPAAX_PROP(MaxSlopeAngleDeg).SetRange(0.f, 89.f))

        /** cos(MaxSlopeAngleDeg): the minimum surface normal Y that counts as ground. */
        float GroundNormalY() const noexcept
        {
            return Maths::Cos(Maths::DegreesToRadians(MaxSlopeAngleDeg));
        }

        // =============================================================================
        // Step
        // =============================================================================
    public:
        /** Advances one entity by InContext.DeltaTime. */
        virtual void Tick(MoverTickContext& InContext) const = 0;

        /**
         * Called by MoverSubsystem when an entity switches modes. DeltaTime is 0 and nothing is swept.
         * Override to reset per-entity state on the component.
         */
        virtual void OnModeEnter(MoverTickContext& /*InContext*/) const {}
        virtual void OnModeExit(MoverTickContext& /*InContext*/) const {}
    };
}
