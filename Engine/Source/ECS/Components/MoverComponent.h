#pragma once

#include "Core/Component/OpaaxComponent.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxStringID.hpp"

#include "Assets/AssetHandle.hpp"
#include "ECS/Components/MoverTypes.h"
#include "Physics/PhysicsTypes.h"
#include "Physics/Collision/CollisionChannel.h"

namespace Opaax
{
    class CollisionProfile;
    class IMoverModeParams;
}

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
        // All out-of-line (the per-mode params UniquePtr map needs the complete IMoverModeParams type).
        MoverComponent();
        explicit MoverComponent(const json& Json);
        ~MoverComponent() override;

        // Move-only: owns its per-mode params by UniquePtr. Copy is deleted; if a copy is ever needed,
        // add a Clone-based copy ctor (IMoverModeParams::Clone exists for exactly that).
        MoverComponent(const MoverComponent&)            = delete;
        MoverComponent& operator=(const MoverComponent&) = delete;
        MoverComponent(MoverComponent&&) noexcept;
        MoverComponent& operator=(MoverComponent&&) noexcept;

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

        // Channels that are SOLID to movement (the geometric sweep slides on them). Profile's Block
        // channels when a profile is set+loaded, else the raw CollisionMask.
        Uint64 EffectiveMovementMask() const noexcept;

        // Channels the mover's body interacts with for EVENTS + presence (Block + Overlap). Profile's
        // ComputeMaskBits when set+loaded, else the raw CollisionMask.
        Uint64 EffectiveEventMask() const noexcept;

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

        // Editor-facing: add InMode to the supported set (also mints its default params via the mode
        // registry) / remove it (and drops its params). Defined in the .cpp (needs the mode registry).
        void AddMode(OpaaxStringID InMode);
        void RemoveMode(OpaaxStringID InMode);

        // Mint default params for any supported mode that lacks them (covers code-created movers that
        // never went through AddMode/deserialize). Called by the subsystem on play-begin.
        void EnsureModeParams();

        // The active-or-given mode's params (downcast by the mode to its concrete type). Null if the
        // mode has no params yet (the subsystem reconciles defaults on play-begin).
        IMoverModeParams* GetModeParams(OpaaxStringID InMode) const noexcept
        {
            const auto lIt = ModeParams.find(InMode.GetId());
            return lIt != ModeParams.end() ? lIt->second.get() : nullptr;
        }

        // =============================================================================
        // Members
        // =============================================================================
        // ---- Collision proxy (serialized) ----
        EMoverShape Shape  = EMoverShape::Capsule;
        float       Height = 100.f;   // total capsule height (world units); ignored for Circle
        float       Radius = 25.f;    // capsule/circle radius (world units)

        // Raw filter fallback used when Profile is unset. Default = the world (WorldStatic|WorldDynamic).
        Uint64 CollisionMask = CategoryBit(ECollisionChannel::WorldStatic)
                             | CategoryBit(ECollisionChannel::WorldDynamic);

        // Collision behaviour asset. When valid+loaded it drives BOTH the movement mask (Block channels)
        // and the body's event/presence filter (Block + Overlap) — see EffectiveMovementMask/EventMask.
        // Mirrors ColliderComponent::Profile.
        TAssetHandle<CollisionProfile> Profile;

        // Steepest walkable slope; surfaces steeper than this don't count as ground.
        float MaxSlopeAngleDeg = 50.f;

        // ---- Mode selector (serialized) ----
        // The modes this mover SUPPORTS (the authored subset of the global MoverModeRegistry). The
        // active mode + any QueueNextMode are constrained to this set. Default = ground only; the
        // designer adds the modes a given mover needs (Fly, Swim, ...).
        TDynArray<OpaaxStringID> Modes = { OPAAX_ID("GroundMove") };

        // The currently-active mode (must be one of Modes). Data-driven; default = the ground mode.
        OpaaxStringID ModeId = OPAAX_ID("GroundMove");

        // ---- Per-mode tuning (serialized) ----
        // One params instance per supported mode, keyed by mode-id. Each mode owns its concrete
        // IMoverModeParams type; this component only stores/serializes the base (the mode downcasts).
        UnorderedMap<Uint32, UniquePtr<IMoverModeParams>> ModeParams;

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
