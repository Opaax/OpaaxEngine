#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class Entity;
    class World;

    // =============================================================================
    // EntityQuery — an entity's bounds in world space, and what is at a point or in a region.
    //   Used by picking, the selection outline, focus and the marquee.
    // =============================================================================
    namespace EntityQuery
    {
        /**
         * The axis-aligned box covering everything the entity draws, in world units.
         * An entity with nothing to draw gets a box of InAnchorHalfExtent (so it can be clicked).
         * @param InAnchorHalfExtent Half-size of that box. Zero = no fallback.
         * @return False if the entity is invalid, has no transform, or has no bounds.
         *   OutBounds is untouched then.
         */
        bool TryGetBounds(Entity InEntity, Bounds2D& OutBounds, float InAnchorHalfExtent = 0.f);

        /** The box covering all of InIds. False if none had bounds. */
        bool TryGetBounds(World& InWorld, const TDynArray<EntityID>& InIds, Bounds2D& OutBounds,
                                    float InAnchorHalfExtent = 0.f);

        /**
         * The topmost entity at InWorldPoint (in draw order), or an invalid Entity.
         */
        Entity PickAt(World& InWorld, const Vector2F& InWorldPoint, float InAnchorHalfExtent = 0.f);

        /**
         * Every entity overlapping InRegion (marquee). Appends to OutIds.
         */
        void QueryOverlapping(World& InWorld, const Bounds2D& InRegion, TDynArray<EntityID>& OutIds,
                                        float InAnchorHalfExtent = 0.f);
    }
}
