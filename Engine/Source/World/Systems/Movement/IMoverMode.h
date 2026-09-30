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
    // MoverTickContext — what a mode needs for one entity's step. Params is the mode's tuning asset.
    // =============================================================================
    struct MoverTickContext
    {
        IPhysicsWorld&      World;
        MoverComponent&     Mover;
        TransformComponent& Transform;
        const MoveModeData& Params;

        float DeltaTime = 0.f;

        /**
         * The mover's own body user data (so the sweep skips it). 0 on transition calls.
         */
        Uint64 SelfUserData = 0;
    };

    // =============================================================================
    // IMoverMode — a movement behaviour (gravity, acceleration, jump, ...).
    //   Reads intent and state from the component, calls MoveCapsule, writes the result.
    //   Stateless: per-entity state is on the component, so one instance serves every entity.
    //   New movement = a new mode.
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
        /** Advances one entity by InContext.DeltaTime. */
        virtual void Tick(MoverTickContext& InContext) = 0;

        // =============================================================================
        // Transitions — called by MoverSubsystem when an entity switches modes
        // =============================================================================
    public:
        /**
         * DeltaTime is 0 and nothing is swept. Override to reset per-entity state on the component.
         */
        virtual void OnModeEnter(MoverTickContext& /*InContext*/) {}
        virtual void OnModeExit(MoverTickContext& /*InContext*/) {}
    };
}
