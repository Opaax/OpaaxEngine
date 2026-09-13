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
    // EntityHierarchy — WHO an entity hangs off, and WHERE that puts it in the world.
    //
    //   The link is EntityMeta::Parent, a Guid (identity, not a component — see the contract's
    //   §HR). TransformComponent is LOCAL to that parent; a root's local is its world. The world
    //   pose is WALKED here on demand, never cached: no dirty flag, no propagation order, and an
    //   Inspector edit is visible to the pick on the same frame. A cache is the growth point,
    //   gated on measuring it.
    //
    //   Children are DERIVED, never stored — a scan over EntityMeta, the same rule that makes a
    //   Map a partition rather than a container (WM2).
    // =============================================================================
    namespace EntityHierarchy
    {
        /** Chain depth beyond which a walk stops — a corrupted link must not hang the frame. */
        inline constexpr Uint32 MAX_DEPTH = 256;

        /** The parent, or an invalid Entity for a root — or for a link that resolves to nothing. */
        OPAAX_API Entity GetParent(Entity InEntity);

        /**
         * Hang InChild under InParent; an invalid InParent DETACHES it.
         *
         * Keeps the WORLD pose by default (Unity's `worldPositionStays`, Godot's
         * `keep_global_transform`): the local is recomputed so nothing visibly moves.
         *
         * A CHILD LIVES IN ITS PARENT'S MAP: when InParent belongs to a map, the whole subtree's
         * OwnerMap follows it. A parent with no map (runtime-spawned) leaves the child's alone.
         *
         * @return false — with a Warn — for an invalid child, a parent in another world, self, or
         *   a parent that is already a descendant (a cycle). Nothing is changed in that case.
         */
        OPAAX_API bool SetParent(Entity InChild, Entity InParent, bool bInKeepWorld = true);

        /** True when InAncestor is anywhere on InEntity's parent chain (InEntity itself excluded). */
        OPAAX_API bool IsDescendantOf(Entity InEntity, Entity InAncestor);

        /**
         * InRoots plus every descendant, each once, parents before their children.
         * What a Delete destroys, an undo step captures, and a prefab is made of.
         */
        OPAAX_API void CollectSubtree(World& InWorld, const TDynArray<EntityID>& InRoots,
                                      TDynArray<EntityID>& OutIds);

        /**
         * InIds minus every entity that has an ancestor in InIds — the ones a world-space delta
         * must touch directly. A selected child of a selected parent moves with it, not twice.
         */
        OPAAX_API void TopmostOf(World& InWorld, const TDynArray<EntityID>& InIds,
                                 TDynArray<EntityID>& OutIds);

        /** The pose in world space: the chain composed root → leaf. A root answers its own transform. */
        OPAAX_API TransformComponent WorldTransform(Entity InEntity);

        /** Store InWorld as the LOCAL that puts the entity there under its current parent. */
        OPAAX_API void SetWorldTransform(Entity InEntity, const TransformComponent& InWorld);

        /**
         * Every direct child of InParent, by guid compare over the whole registry. O(n) by
         * design; a children list would be a second store to keep in step.
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
