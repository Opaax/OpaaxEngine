#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IPhysicsWorld;
    struct MoveModeData;
    struct MoverComponent;
    struct TransformComponent;

    // =============================================================================
    // MoverTickContext — everything a mode needs for one entity's step.
    //
    //   Params is the TUNING RESOURCE the mover's bag resolved for this mode, handed over as a
    //   plain reference. M9 passed a type-erased IMoverModeParams* here and every mode opened with
    //   an assert and a downcast; the tuning being a RESOURCE deleted both, along with the type
    //   tag that made the downcast checkable.
    // =============================================================================
    struct MoverTickContext
    {
        IPhysicsWorld&      World;
        MoverComponent&     Mover;
        TransformComponent& Transform;
        const MoveModeData& Params;

        float DeltaTime = 0.f;

        /**
         * The mover's OWN body user-data, so the sweep skips its own kinematic body. 0 on the
         * transition ticks, which do not sweep.
         */
        Uint64 SelfUserData = 0;
    };

    // =============================================================================
    // IMoverMode — a pluggable movement behaviour, and the anti-monolith seam.
    //
    //   A mode owns HOW something moves — its policy: gravity, acceleration, jumping — reads
    //   intent and sync-state from the component, calls MoveCapsule for the geometric solve, and
    //   writes the result back. New movement is a new MODE, never a component subclass and never
    //   a branch inside an existing one.
    //
    //   MODES ARE STATELESS. All per-entity state lives on the component, so one instance serves
    //   every entity running that mode — which is what lets the registry own one of each.
    //
    //   IT HAS NO CreateDefaultParams. M9's modes minted their own params type because the params
    //   lived on the component; now a tuning is an asset the author picks, so a mode reads what it
    //   is handed and has no opinion about where it came from.
    // =============================================================================
    class OPAAX_API IMoverMode
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IMoverMode() = default;

        // =============================================================================
        // Tick
        // =============================================================================
    public:
        /** Advance one entity's movement by InContext.DeltaTime. */
        virtual void Tick(MoverTickContext& InContext) = 0;

        // =============================================================================
        // Transitions — fired by MoverSubsystem when an entity switches modes
        // =============================================================================
    public:
        /**
         * The context carries DeltaTime = 0 and does not sweep. Default no-op; a mode overrides
         * these to reset per-entity state on the component (dropping carried-over momentum, say).
         * The stateless contract still holds — they only touch the entity in the context.
         */
        virtual void OnModeEnter(MoverTickContext& /*InContext*/) {}
        virtual void OnModeExit(MoverTickContext& /*InContext*/) {}
    };
}
