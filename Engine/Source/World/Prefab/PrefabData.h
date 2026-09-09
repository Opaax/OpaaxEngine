#pragma once

#include "World/Serialization/MapData.h"

namespace Opaax
{
    // =============================================================================
    // PrefabData — a prefab's entities as DATA, detached from any World.
    //
    //   THE SAME EntityData A MAP HOLDS (⑦-C **K1**). A Map is pure entity data with no systems
    //   and no runtime ownership (**WM1**), and a prefab is that same data without a map's
    //   membership — which is what lets MapFactory::Instantiate and MapFactory::Restore serve a
    //   prefab with no prefab-specific code at all, and what makes EntityJson one writer for both.
    //
    //   NO `prefabId`, AND THE ASYMMETRY WITH MapData::Id (**MP10**) IS DELIBERATE. A map names
    //   itself for two reasons, neither of which holds here: its entities carry `OwnerMap` and the
    //   two must agree, and an invalid `MapId` doubles as "no filter, take the whole world".
    //   Nothing partitions entities by prefab — an instance names its prefab by PATH — so an id
    //   field would be a second name for one thing, stale the moment the file is renamed.
    //
    //   A PREFAB'S ENTITIES CARRY AN INVALID `OwnerMap`. They belong to no map until an instance
    //   stamps one (PrefabFactory::BuildInstance), which is the same rule WM2 already states:
    //   default-invalid means "no map authored this".
    //
    //   Plain aggregate, no OPAAX_API: no vtable, no out-of-line members, nothing to export.
    // =============================================================================
    struct PrefabData
    {
        TDynArray<EntityData> Entities;

        bool   IsEmpty()     const noexcept { return Entities.empty(); }
        Uint64 EntityCount() const noexcept { return static_cast<Uint64>(Entities.size()); }
    };
}
