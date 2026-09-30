#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityTypes.h"
#include "World/World.h"

namespace Opaax
{
    // =============================================================================
    // EntityHierarchy — parent/child links (EntityMeta::Parent) and world transforms.
    //   TransformComponent is local to the parent. World poses are computed on demand (no cache).
    //   Children are found by scanning, not stored.
    // =============================================================================
    namespace EntityHierarchy
    {
        /** Maximum chain depth, so a broken link cannot hang. */
        inline constexpr Uint32 MAX_DEPTH = 256;

        /** The parent, or an invalid Entity for a root. */
        OPAAX_API Entity GetParent(Entity InEntity);

        /**
         * Puts InChild under InParent; an invalid InParent detaches it. Keeps the world pose.
         * The subtree moves to the parent's map (if it has one).
         * @return False (with a warning) for an invalid child, a parent in another world, self,
         *   or a cycle. Nothing changes then.
         */
        OPAAX_API bool SetParent(Entity InChild, Entity InParent, bool bInKeepWorld = true);

        /** True if InAncestor is on InEntity's parent chain (InEntity excluded). */
        OPAAX_API bool IsDescendantOf(Entity InEntity, Entity InAncestor);

        /**
         * InRoots and all their descendants, each once, parents first.
         */
        OPAAX_API void CollectSubtree(World& InWorld, const TDynArray<EntityID>& InRoots,
                                      TDynArray<EntityID>& OutIds);

        /**
         * InIds without the entities that have an ancestor in InIds (so a moved child is not moved twice).
         */
        OPAAX_API void TopmostOf(World& InWorld, const TDynArray<EntityID>& InIds,
                                 TDynArray<EntityID>& OutIds);

        /** The world pose (the chain composed root to leaf). */
        OPAAX_API TransformComponent WorldTransform(Entity InEntity);

        /**
         * The chain composed root to leaf, with InLocalOf(Entity) giving each local.
         */
        template<typename TLocalOf>
        TransformComponent ComposeChain(Entity InEntity, TLocalOf&& InLocalOf)
        {
            TransformComponent lWorld;
            if (!InEntity.IsValid()) { return lWorld; }

            Entity lChain[MAX_DEPTH];
            Uint32 lCount = 0;

            for (Entity lCursor = InEntity; lCursor.IsValid() && lCount < MAX_DEPTH; lCursor = GetParent(lCursor))
            {
                lChain[lCount++] = lCursor;
            }

            // A root's local is its world.
            if (lCount == 1) { return InLocalOf(InEntity); }

            for (Uint32 lIndex = lCount; lIndex > 0; --lIndex)
            {
                lWorld = Compose(lWorld, InLocalOf(lChain[lIndex - 1]));
            }

            return lWorld;
        }

        /** Sets the local so the entity ends up at InWorld. */
        OPAAX_API void SetWorldTransform(Entity InEntity, const TransformComponent& InWorld);

        /**
         * Every direct child of InParent (scans the registry).
         */
        template<typename TFunc>
        void ForEachChild(World& InWorld, const EntityID InParent, TFunc&& InFunc)
        {
            const EntityMeta* lMeta = InWorld.GetRegistry().try_get<EntityMeta>(InParent);
            if (lMeta == nullptr) { return; }

            const Guid lParentId = lMeta->Id;

            InWorld.Each<EntityMeta>([&](EntityID InId, const EntityMeta& InChild)
            {
                if (InChild.Parent == lParentId) { InFunc(InId); }
            });
        }
    }
}
