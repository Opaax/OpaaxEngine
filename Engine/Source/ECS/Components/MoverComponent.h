#pragma once

#include "Core/Component/OpaaxComponent.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxStringID.hpp"

#include "ECS/Components/MoverTypes.h"
#include "Physics/PhysicsTypes.h"
#include "Physics/Collision/CollisionChannel.h"

namespace Opaax::ECS
{
    using json = nlohmann::json;

    // =============================================================================
    // MoverComponent
    // =============================================================================
    /**
     * @struct MoverComponent
     *
     * Generic kinematic mover (Mover-2.0-style): dumb data that an IMoverMode drives. NOT a
     * simulated rigid body — it sweeps a capsule/circle proxy against the world (collide-and-slide)
     * and resolves its own Transform. Carry NO ColliderComponent on the same entity (that would
     * build a sim body too); the mover is geometric.
     *
     * Behaviour is NOT here — it lives in the IMoverMode selected by ModeId, so this component
     * never grows movement logic (the anti-CharacterMovementComponent stance). Intent is written
     * by a producer (controller/AI/script) into Input; the mode consumes it.
     */
    struct OPAAX_API MoverComponent : public OpaaxComponentBase
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        MoverComponent()          = default;
        explicit MoverComponent(const json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
        virtual ~MoverComponent() = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin OpaaxComponentBase Interface
    protected:
        void DeserializeImplementation(const json& Json) override;

    public:
        json Serialize() const override;
        //~ End OpaaxComponentBase Interface

        // =============================================================================
        // Helpers
        // =============================================================================
    public:
        // Build the local-space sweep capsule from Shape/Height/Radius (Circle => degenerate,
        // Center1 == Center2; vertical Capsule otherwise). Consumed by the mover mode each step.
        MoverCapsule BuildCapsule() const noexcept;

        // Minimum surface-normal Y that counts as ground = cos(MaxSlopeAngleDeg).
        float GroundNormalY() const noexcept;

        // Mover-2.0-style deferred mode switch: queue InMode to become active on the next fixed
        // step (the MoverSubsystem fires OnModeExit/OnModeEnter then). bReenter forces the mode to
        // restart (re-fire OnModeEnter) even if it's already the current mode. Producer-facing API —
        // gameplay code calls this; it never mutates ModeId directly.
        void QueueNextMode(OpaaxStringID InMode, bool bReenter = false) noexcept
        {
            PendingMode    = InMode;
            PendingReenter = bReenter;
        }

        // The currently-active mode (the one ticking). A queued switch isn't reflected here until
        // the subsystem applies it next step.
        OpaaxStringID GetCurrentMode() const noexcept { return ModeId; }

        // Does this mover support InMode (is it in the authored Modes set)? QueueNextMode is
        // constrained to supported modes — a walker can't flip to Fly unless Fly was added.
        bool SupportsMode(OpaaxStringID InMode) const noexcept
        {
            for (const OpaaxStringID& lId : Modes) { if (lId == InMode) { return true; } }
            return false;
        }

        // Editor-facing: add InMode to the supported set (no duplicates) / remove it.
        void AddMode(OpaaxStringID InMode)
        {
            if (!SupportsMode(InMode)) { Modes.push_back(InMode); }
        }
        void RemoveMode(OpaaxStringID InMode)
        {
            for (size_t i = 0; i < Modes.size(); ++i)
            {
                if (Modes[i] == InMode) { Modes.erase(Modes.begin() + i); return; }
            }
        }

        // =============================================================================
        // Members
        // =============================================================================
        // ---- Collision proxy (serialized) ----
        EMoverShape Shape  = EMoverShape::Capsule;
        float       Height = 100.f;   // total capsule height (world units); ignored for Circle
        float       Radius = 25.f;    // capsule/circle radius (world units)

        // Which channels are SOLID to the mover (it slides against them). Default = the world
        // (WorldStatic | WorldDynamic). The mover is a query, not a shape — it has no own channel.
        Uint64 CollisionMask = CategoryBit(ECollisionChannel::WorldStatic)
                             | CategoryBit(ECollisionChannel::WorldDynamic);

        // Steepest walkable slope; surfaces steeper than this don't count as ground.
        float MaxSlopeAngleDeg = 50.f;

        // ---- Mode selector (serialized) ----
        // The modes this mover SUPPORTS (the authored subset of the global MoverModeRegistry). The
        // active mode + any QueueNextMode are constrained to this set. Default = ground only; the
        // designer adds the modes a given mover needs (Fly, Swim, ...).
        TDynArray<OpaaxStringID> Modes = { OPAAX_ID("GroundMove") };

        // The currently-active mode (must be one of Modes). Data-driven; default = the ground mode.
        OpaaxStringID ModeId = OPAAX_ID("GroundMove");

        // ---- Tuning (serialized) ----
        MoverParams Params;

        // ---- Sync state (runtime, NOT serialized) ----
        Vector2F Velocity     = { 0.f, 0.f };
        bool     Grounded     = false;
        Vector2F GroundNormal = { 0.f, 0.f };

        // ---- Intent (runtime, producer-written, NOT serialized) ----
        MoverInput Input;

        // ---- Queued mode switch (runtime, NOT serialized) ----
        // Set by QueueNextMode; consumed + cleared by the MoverSubsystem next step. Invalid
        // (ID_None) means no pending switch.
        OpaaxStringID PendingMode;
        bool          PendingReenter = false;
    };
}
