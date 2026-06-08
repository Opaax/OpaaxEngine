#pragma once

#include "Core/EngineAPI.h"

namespace Opaax
{
    class IPhysicsWorld;

    namespace ECS
    {
        struct MoverComponent;
        struct TransformComponent;
    }

    // =============================================================================
    // MoverTickContext
    // =============================================================================
    /**
     * @struct MoverTickContext
     *
     * Everything a mover mode needs for one entity's step: the physics world to sweep against,
     * the entity's MoverComponent (intent in, sync-state out), its Transform (position out), and
     * the fixed delta. The MoverSubsystem builds this per entity and hands it to the active mode.
     */
    struct MoverTickContext
    {
        IPhysicsWorld&           World;
        ECS::MoverComponent&     Mover;
        ECS::TransformComponent& Transform;
        float                    DeltaTime;
    };

    // =============================================================================
    // IMoverMode
    // =============================================================================
    /**
     * @interface IMoverMode
     *
     * A pluggable movement behaviour — the anti-monolith seam. A mode owns "how this thing moves"
     * (its policy: gravity/acceleration/jump/etc.), reads intent + sync-state from the component,
     * calls World.MoveCapsule for the geometric solve, and writes the result back. Modes are
     * STATELESS (all per-entity state lives on the component), so one instance serves every entity
     * running that mode. Register modes by id on the MoverSubsystem; new movement = a new mode,
     * never a component subclass.
     */
    class OPAAX_API IMoverMode
    {
    public:
        virtual ~IMoverMode() = default;

        // Advance one entity's movement by ctx.DeltaTime.
        virtual void Tick(MoverTickContext& InContext) = 0;

        // Transition lifecycle, fired by the MoverSubsystem when an entity switches modes
        // (MoverComponent::QueueNextMode). The context carries DeltaTime = 0. Default no-op —
        // a mode overrides these to (re)initialise or clean up per-entity sync-state on the
        // component (e.g. zero residual velocity on enter). Stateless contract still holds:
        // they only read/write the per-entity component in the context.
        virtual void OnModeEnter(MoverTickContext& /*InContext*/) {}
        virtual void OnModeExit (MoverTickContext& /*InContext*/) {}
    };

} // namespace Opaax
