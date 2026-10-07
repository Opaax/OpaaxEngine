#pragma once

#include "Core/Maths/MathTypes.h"
#include "World/Entity/Entity.h"

namespace Opaax
{
    // =============================================================================
    // Entity events the engine sends to behaviours (Listen<&MyBehaviour::OnX>()).
    //   Physics sends each to BOTH entities involved, right after the fixed step that produced it.
    // =============================================================================

    /** Another solid collider started touching this entity's. */
    struct CollisionBegan
    {
        Entity Other;
    };

    /** Another solid collider stopped touching this entity's. */
    struct CollisionEnded
    {
        Entity Other;
    };

    /**
     * Another collider started overlapping this entity's. bIsSensor: this entity owns the overlap
     * (sensor) collider; false means this entity entered Other's sensor.
     */
    struct OverlapBegan
    {
        Entity Other;
        bool   bIsSensor = false;
    };

    /** The overlap with Other ended. Same fields as OverlapBegan. */
    struct OverlapEnded
    {
        Entity Other;
        bool   bIsSensor = false;
    };

    /** This entity left the world bounds. With the destroying response it ends right after. */
    struct ExitedWorldBounds
    {
        Vector2F LastPosition = { 0.f, 0.f };
    };
}
