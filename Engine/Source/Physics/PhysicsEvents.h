#pragma once

#include "Core/Maths/MathTypes.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class World;

    // =============================================================================
    // PhysicsEvents.h — events published on the EventBus by PhysicsSubsystem after each step.
    //   Published immediately (not enqueued), right after the step that produced them.
    //   A handler may destroy an entity: its body is removed next step and its pairs are dropped.
    // =============================================================================

    // =============================================================================
    // Overlap — a collider in Overlap mode (a sensor)
    // =============================================================================
    /**
     * A visitor entered a sensor. OverlapEntity owns the sensor; OtherEntity entered it.
     */
    struct PhysicsOverlapBegan
    {
        EntityID OverlapEntity = ENTITY_NONE;
        EntityID OtherEntity   = ENTITY_NONE;

        /** The world whose physics produced the event. */
        const World* SourceWorld = nullptr;
    };

    /**
     * A visitor is still inside a sensor: once per fixed step between Began and Ended.
     */
    struct PhysicsOverlapStayed
    {
        EntityID OverlapEntity = ENTITY_NONE;
        EntityID OtherEntity   = ENTITY_NONE;

        /** The world whose physics produced the event. */
        const World* SourceWorld = nullptr;
    };

    /** A visitor left a sensor. Same fields as Began. */
    struct PhysicsOverlapEnded
    {
        EntityID OverlapEntity = ENTITY_NONE;
        EntityID OtherEntity   = ENTITY_NONE;

        /** The world whose physics produced the event. */
        const World* SourceWorld = nullptr;
    };

    // =============================================================================
    // Collision — two solid colliders
    // =============================================================================
    /**
     * Two solid colliders started touching. A and B have no particular meaning.
     */
    struct PhysicsCollisionBegan
    {
        EntityID EntityA = ENTITY_NONE;
        EntityID EntityB = ENTITY_NONE;

        /** The world whose physics produced the event. */
        const World* SourceWorld = nullptr;
    };

    /** Two solid colliders stopped touching. Same fields as Began. */
    struct PhysicsCollisionEnded
    {
        EntityID EntityA = ENTITY_NONE;
        EntityID EntityB = ENTITY_NONE;

        /** The world whose physics produced the event. */
        const World* SourceWorld = nullptr;
    };

    // =============================================================================
    // World bounds — the optional kill volume
    // =============================================================================
    /**
     * A dynamic body left the world bounds. Fires once per exit, whatever the configured response.
     * With EventAndDestroy the entity is destroyed after this event (still alive for handlers).
     * LastPosition is where it left.
     */
    struct PhysicsExitedWorldBounds
    {
        EntityID Entity       = ENTITY_NONE;
        Vector2F LastPosition = { 0.f, 0.f };

        /** The world whose physics produced the event. */
        const World* SourceWorld = nullptr;
    };
}
