#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Prefab/PrefabData.h"
#include "World/Serialization/MapData.h"

namespace Opaax
{
    class ComponentRegistry;
    class IPrefabResolver;

    inline constexpr LogCategory LogPrefabFactory{"PrefabFactory"};

    // =============================================================================
    // PrefabFactory — converts between prefabs and instances. Pure functions, no World.
    //   The result of BuildInstance goes to MapFactory::Instantiate.
    // =============================================================================
    class PrefabFactory
    {
    public:
        /**
         * The entities of one instance of InPrefab, ready for MapFactory::Instantiate:
         *   - Guid = Guid::Derive(InInstanceId, prefab guid), so instances do not collide;
         *   - OwnerMap = InOwnerMap, so they are saved with the map;
         *   - a PrefabInstanceComponent naming the prefab, the instance and the template guid.
         * Refused (empty result, error) if InInstanceId or InOwnerMap is invalid, or if
         * PrefabInstanceComponent is not registered.
         * @param InPrefabAssetPath Asset-relative ("Prefabs/X.opaaxprefab")
         * @param InInstanceId Identifies this placement (Guid::New per instantiate)
         * @return The instance's entities (MapData::Id is InOwnerMap)
         */
        static MapData BuildInstance(const PrefabData& InPrefab, const OpaaxString& InPrefabAssetPath,
                                     const Guid& InInstanceId, MapId InOwnerMap,
                                     const ComponentRegistry& InRegistry);

        /**
         * The inverse: captured entities become a prefab.
         *   - OwnerMap is cleared;
         *   - PrefabInstanceComponents are removed (nested placements come as records, fold first).
         * Guids are kept: they become the template guids.
         * @return The prefab. Empty if InCaptured is empty.
         */
        static PrefabData BuildPrefab(const MapData& InCaptured, const ComponentRegistry& InRegistry);

        /**
         * The prefab as consumers see it: its own entities plus every nested placement expanded,
         * OwnerMap cleared, nested markers removed. A variant becomes its base with the overrides applied.
         * Cycles are refused by the resolver.
         */
        static PrefabData Flatten(const PrefabData& InRaw, const OpaaxString& InPrefabAssetPath,
                                  const IPrefabResolver& InResolver, const ComponentRegistry& InRegistry);

        /**
         * InState as a variant of the prefab at InBaseAssetPath: the base's entities become one record
         * with overrides (a patch per changed entity, null per removed one); everything else stays
         * the variant's own. Entities are matched to the base by guid. The base file is not changed.
         * @param InState Captured entities, expanded
         * @return The variant. Empty (with an error) if the base cannot be resolved.
         */
        static PrefabData BuildVariant(const MapData& InState, const OpaaxString& InBaseAssetPath,
                                       const IPrefabResolver& InResolver, const ComponentRegistry& InRegistry);
    };
}
