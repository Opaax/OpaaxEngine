#pragma once

#include "Core/Maths/MathTypes.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // PhysicsEvents.h — Tier-3 bus payloads published by PhysicsSubsystem after each step.
    //
    //   PLAIN PODS, like WorldEvents.h beside them: adding a physics event is adding a struct
    //   here, with no central registration and no enum entry. The Tier-1 EEventType list stays
    //   closed to OS/window/input, exactly as its own comment says.
    //
    //   THEY ARE PUBLISHED, NOT ENQUEUED, and that is a deliberate departure from the bus's
    //   stated default. `EventBus::Flush` runs at the TOP of `Engine::Loop`, before Update and
    //   before the fixed-step catch-up loop — so an ENQUEUED contact would be delivered a whole
    //   frame later, after the reconcile had already run and after any number of further steps.
    //   A frame that ran three fixed steps would deliver three steps' worth of Began/Ended pairs
    //   in one batch with no relationship to the simulation that produced them. Immediate
    //   dispatch puts a handler right where M9 proved it works: after the Step that produced the
    //   touch, with both entities still live.
    //
    //   A HANDLER MAY DESTROY AN ENTITY. That is the pickup case and it is supported: the next
    //   step's ReconcileDeadBodies reaps the body, and a pair whose entity died is dropped rather
    //   than ticked, so a destroyed sensor cannot emit a phantom Stayed or Ended.
    //
    //   Both worlds in a PIE session share the engine bus, but only the ACTIVE one ticks (WS5),
    //   so exactly one of them is ever publishing.
    // =============================================================================

    // =============================================================================
    // Overlap — a collider whose Mode is Overlap (a sensor)
    // =============================================================================
    /**
     * A sensor and a visitor began overlapping. OverlapEntity owns the Overlap-mode collider;
     * OtherEntity is what entered it.
     */
    struct PhysicsOverlapBegan
    {
        EntityID OverlapEntity = ENTITY_NONE;
        EntityID OtherEntity   = ENTITY_NONE;
    };

    /**
     * A sensor and a visitor are STILL overlapping, once per fixed step between Began and Ended.
     *
     * Synthesized by the subsystem, not by the backend — Box2D reports only begin and end touch,
     * so the live-overlap set is what turns those two edges into a state.
     */
    struct PhysicsOverlapStayed
    {
        EntityID OverlapEntity = ENTITY_NONE;
        EntityID OtherEntity   = ENTITY_NONE;
    };

    /** A sensor and a visitor stopped overlapping. Same payload as Began. */
    struct PhysicsOverlapEnded
    {
        EntityID OverlapEntity = ENTITY_NONE;
        EntityID OtherEntity   = ENTITY_NONE;
    };

    // =============================================================================
    // Collision — two colliders whose Mode is Solid
    // =============================================================================
    /**
     * Two solid colliders began touching. A and B follow the backend's shape order, which is
     * stable but carries no meaning — neither is "the one that hit the other".
     */
    struct PhysicsCollisionBegan
    {
        EntityID EntityA = ENTITY_NONE;
        EntityID EntityB = ENTITY_NONE;
    };

    /** Two solid colliders stopped touching. Same payload as Began. */
    struct PhysicsCollisionEnded
    {
        EntityID EntityA = ENTITY_NONE;
        EntityID EntityB = ENTITY_NONE;
    };

    // =============================================================================
    // World bounds — the optional kill volume
    // =============================================================================
    /**
     * A dynamic body left the configured world bounds.
     *
     * Fires ONCE, on the inside-to-outside transition, not every step the body keeps falling — a
     * body that left is a single occurrence, and a per-step version would be unusable in exactly
     * the case it exists for.
     *
     * It fires whatever the configured response is: `EventAndDestroy` reaps the entity AFTER this
     * is published, so a handler still sees a live entity and can read whatever it needs off it.
     * LastPosition is where it was when it crossed out.
     */
    struct PhysicsExitedWorldBounds
    {
        EntityID Entity       = ENTITY_NONE;
        Vector2F LastPosition = { 0.f, 0.f };
    };
}
