#pragma once

#include "Core/GUID/Guid.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    class ComponentRegistry;

    inline constexpr LogCategory LogEntityClone{"EntityClone"};

    // =============================================================================
    // EntityClone — copies of captured entities that can live beside the originals: every identity new,
    //   the links among them kept. Pure (no World): the editor's copy, paste and duplicate build on it.
    // =============================================================================
    namespace EntityClone
    {
        struct Options
        {
            /** The map the copies belong to. Invalid: each copy keeps its entity's. */
            MapId OwnerMap;

            /**
             * A parent outside the copied set: kept, so a copy is its original's sibling (duplicate), or
             * cleared, so it is a root (paste). Clearing does not move it: the caller stores the world pose.
             */
            bool bKeepOutsideParents = false;
        };

        /**
         * InSource's entities with new identities:
         *   - an entity of a prefab instance gets Guid::Derive(new instance id, its template guid), with one
         *     new instance id per instance, so the copy is a placement of its own and a map still folds it;
         *   - any other entity gets Guid::New();
         *   - a parent inside InSource names its copy, and so does an EntityRef to an entity of InSource;
         *     a reference to an entity left out still names that entity.
         * Names, components and order are kept. InSource's folded records are not copied (captures have none).
         */
        MapData Clone(const MapData& InSource, const Options& InOptions, const ComponentRegistry& InRegistry);

        /** The prefab instances InData's entities belong to, each once, in first-seen order. */
        TDynArray<Guid> InstancesOf(const MapData& InData, const ComponentRegistry& InRegistry);

        /** How many of InData's entities belong to the instance InInstanceId. */
        Uint64 CountOfInstance(const MapData& InData, const Guid& InInstanceId, const ComponentRegistry& InRegistry);

        /**
         * Removes the prefab link of InData's entities of the instance InInstanceId: they become plain
         * entities. For a copy of part of an instance, which no prefab describes.
         * @return How many were unlinked
         */
        Uint64 Unlink(MapData& InOutData, const Guid& InInstanceId, const ComponentRegistry& InRegistry);
    }
}
