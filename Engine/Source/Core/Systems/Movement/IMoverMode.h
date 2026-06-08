#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IPhysicsWorld;
    class IMoverModeParams;

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

        // The mover's OWN body user-data (entity-bits + 1) — passed to MoveCapsule so the geometric
        // sweep skips the mover's own kinematic body. 0 during transition (enter/exit) ticks.
        Uint64                   SelfUserData = 0;

        // The active mode's params (the subsystem resolves MoverComponent::GetModeParams(ModeId) and
        // passes it here). The mode downcasts to its own concrete type. Null only if misconfigured.
        IMoverModeParams*        Params = nullptr;
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

        // Factory for THIS mode's params type — the mode is the only thing that knows its concrete
        // IMoverModeParams. Used to seed defaults on a mover and to mint the right type on load.
        virtual UniquePtr<IMoverModeParams> CreateDefaultParams() const = 0;

        // Advance one entity's movement by ctx.DeltaTime (reads ctx.Params, downcast to its type).
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
