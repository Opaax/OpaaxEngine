#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Prefab/PrefabData.h"
#include "World/Serialization/MapData.h"

namespace Opaax
{
    class ComponentRegistry;

    inline constexpr LogCategory LogPrefabFactory{"PrefabFactory"};

    // =============================================================================
    // PrefabFactory — a prefab's entities become ONE INSTANCE's entities.
    //
    //   ONE PUBLIC FUNCTION, AND IT TOUCHES NO WORLD. `BuildInstance` is a pure
    //   PrefabData -> MapData transform; the caller hands the result to the existing
    //   `MapFactory::Instantiate`, which is why the whole instantiate path needed no new
    //   world-facing code (⑦-C **K1**/**K2**).
    //
    //   WHY NOT A MODE ON MapFactory::Instantiate. That function PRESERVES guids and refuses one
    //   already live in the world, which is correct for loading a map beside another and is the
    //   exact thing that makes a second instance of one prefab impossible. Teaching it a
    //   "remap" flag would put two opposite identity policies behind one name; a separate named
    //   transform in FRONT of it keeps `Instantiate` meaning one thing — **MP10**'s
    //   CaptureWorld/CaptureMap split, one layer over.
    //
    //   Stateless, for MapFactory's and MapSerializer's reason.
    // =============================================================================
    class OPAAX_API PrefabFactory
    {
    public:
        /**
         * Build the entities ONE INSTANCE of InPrefab would have, ready for MapFactory::Instantiate.
         *
         * Three things happen to every entity, and each is load-bearing:
         *   - its Guid becomes `Guid::Derive(InInstanceId, <the prefab's guid>)`, so two instances
         *     of one prefab in one world do not collide (**WM3**) and a re-apply lands on the same
         *     entities every time;
         *   - its `OwnerMap` becomes InOwnerMap — a prefab's entities belong to no map (**WM2**),
         *     and an instance's belong to the map it was placed in, or they read as
         *     runtime-spawned and no Save would ever write them;
         *   - it gains a `PrefabInstanceComponent` naming the prefab, the instance and the entity's
         *     own template guid.
         *
         * REFUSED (an EMPTY MapData, with an Error) when InInstanceId is invalid, when InOwnerMap
         * is invalid, or when `PrefabInstanceComponent` is not registered — an instance whose
         * entities carried no marker would be untraceable the moment it existed, which is worse
         * than not creating it.
         *
         * @param InPrefabAssetPath Asset-relative, as a component stores it ("Prefabs/X.opaaxprefab").
         * @param InInstanceId Identifies this PLACEMENT. Mint one per instantiate (`Guid::New`).
         * @return The instance's entities. `MapData::Id` is InOwnerMap. The caller can read the
         *   derived guids off it BEFORE instantiating, which is how it finds the entities
         *   afterwards — `MapFactory::Instantiate` answers only a count.
         */
        static MapData BuildInstance(const PrefabData& InPrefab, const OpaaxString& InPrefabAssetPath,
                                     const Guid& InInstanceId, MapId InOwnerMap,
                                     const ComponentRegistry& InRegistry);
    };
}
