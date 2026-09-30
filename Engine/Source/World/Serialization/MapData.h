#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // MapData — a map's entities as data, detached from any World.
    //       World --MapSerializer::Capture--> MapData --MapFactory::Instantiate--> World
    //   Interned ids are saved as strings (they are not stable across runs).
    // =============================================================================

    // One component's data, keyed by its registered name (not the entt type id, which
    // changes if the C++ type is renamed).
    struct ComponentData
    {
        OpaaxStringID  TypeName;
        nlohmann::json Payload;
    };

    // One entity: its identity plus its registered components. Identity is stored here
    // directly (not as an EntityMeta component).
    struct EntityData
    {
        Guid                     Id;
        OpaaxString              Name;
        MapId                    OwnerMap;
        Guid                     Parent;      // invalid = root
        TDynArray<ComponentData> Components;
    };

    // One instance entity's changes from its template.
    struct PrefabOverrideEntry
    {
        Guid           TemplateGuid;   // which entity of the prefab
        nlohmann::json Patch;          // PrefabOverrides format
    };

    // =============================================================================
    // PrefabInstanceRecord — one prefab placement as a map file stores it: the prefab, the
    //   instance id, and only the changes. Prefab edits reach every instance on the next load.
    //   Converted to and from entities by PrefabFold.
    // =============================================================================
    struct PrefabInstanceRecord
    {
        /** Asset-relative ("Prefabs/Turret.opaaxprefab"). */
        OpaaxString Prefab;

        /** Identifies this placement (used by Guid::Derive). */
        Guid InstanceId;

        /** Only the changed entities. */
        TDynArray<PrefabOverrideEntry> Overrides;
    };

    struct MapData
    {
        /**
         * Which map this is. Set when loading: the mapId key, else what the entities claim, else
         * the file name. Can be assumed valid above the loading code.
         */
        MapId Id;

        TDynArray<EntityData> Entities;

        /**
         * The folded prefab placements. Only filled on the way to/from a .opaaxmap; captures,
         * Play copies and undo records keep instance entities expanded.
         */
        TDynArray<PrefabInstanceRecord> Instances;

        /** True when there are no entities and no placements. */
        bool   IsEmpty()      const noexcept { return Entities.empty() && Instances.empty(); }
        Uint64 EntityCount()  const noexcept { return static_cast<Uint64>(Entities.size()); }

        /** Number of folded placements. */
        Uint64 InstanceCount() const noexcept { return static_cast<Uint64>(Instances.size()); }

        /**
         * The first valid OwnerMap among the entities, or invalid. For old files without a mapId;
         * use Id.
         */
        MapId OwnerId() const noexcept
        {
            for (const EntityData& lEntity : Entities)
            {
                if (lEntity.OwnerMap.IsValid()) { return lEntity.OwnerMap; }
            }

            return MapId();
        }
    };
}
