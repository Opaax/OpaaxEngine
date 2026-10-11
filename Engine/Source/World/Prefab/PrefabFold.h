#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Prefab/PrefabData.h"
#include "World/Serialization/MapData.h"

namespace Opaax
{
    class ComponentRegistry;

    inline constexpr LogCategory LogPrefabFold{"PrefabFold"};

    // =============================================================================
    // IPrefabResolver — returns a prefab's data from its path. Implemented by the caller
    //   (Level, the editor), since it needs IPaths and the ResourceManager.
    // =============================================================================
    class IPrefabResolver
    {
    public:
        virtual ~IPrefabResolver() = default;

        /** @return The prefab's entities, or null if the path cannot be loaded */
        virtual const PrefabData* Resolve(const OpaaxString& InAssetPath) const = 0;
    };

    // =============================================================================
    // PrefabFold — prefab instance entities <-> instance records.
    //   Fold on save to a .opaaxmap, Expand on load. Nothing else needs to know about prefabs.
    // =============================================================================
    class PrefabFold
    {
    public:
        /**
         * Replaces instance entities in InOutData with instance records (what files store).
         * Entities are grouped by InstanceId and diffed against their template; only changes are kept.
         * An unresolvable prefab, or an entity not in its prefab, stays expanded (with a warning),
         * so no data is lost.
         * @return Number of records produced. Folding twice changes nothing.
         */
        static Uint64 Fold(MapData& InOutData, const IPrefabResolver& InResolver,
                           const ComponentRegistry& InRegistry);

        /**
         * The inverse: rebuilds every record as entities (PrefabFactory::BuildInstance, same guids),
         * applies the overrides, and clears the record list.
         * An unresolvable prefab loses that placement (with an error).
         * @return Number of placements expanded
         */
        static Uint64 Expand(MapData& InOutData, const IPrefabResolver& InResolver,
                             const ComponentRegistry& InRegistry);
    };
}
