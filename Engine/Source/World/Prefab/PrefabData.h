#pragma once

#include "World/Serialization/MapData.h"

namespace Opaax
{
    // =============================================================================
    // PrefabData — a prefab's entities (same EntityData as a map), with no map ownership
    //   (OwnerMap invalid until instantiated). Has no id: instances name it by path.
    //
    //   It can also place other prefabs (Instances). A file with no entities and one
    //   placement is a variant. Consumers read the flattened prefab through IPrefabResolver.
    // =============================================================================
    struct PrefabData
    {
        TDynArray<EntityData>           Entities;
        TDynArray<PrefabInstanceRecord> Instances;

        bool   IsEmpty()       const noexcept { return Entities.empty() && Instances.empty(); }
        Uint64 EntityCount()   const noexcept { return static_cast<Uint64>(Entities.size()); }
        Uint64 InstanceCount() const noexcept { return static_cast<Uint64>(Instances.size()); }
    };
}
