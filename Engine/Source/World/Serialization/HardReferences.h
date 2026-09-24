#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class ComponentRegistry;
    struct MapData;

    /** One resource a set of entities must have RESIDENT: which type, and which asset. */
    struct HardReference
    {
        Uint32      TypeId = 0;   // ResourceTypeID::Get<T>() — what turns back into a typed load
        OpaaxString Path;         // asset-relative or a mount, as the field was authored
    };

    // =============================================================================
    // HardReferences — what a map's entities must have loaded before they run (⑦-C P5b).
    //
    //   PURE and headless: it reads the entity DATA, not a world, so a map and a prefab are walked
    //   by the same function and a test needs no GL. Each component's hard fields come from the
    //   registry (**PF11** — derived from the type at registration, no list anywhere), and the
    //   field's value is read out of the untyped payload by that name.
    //
    //   Who HOLDS them is the caller's business — a mounted map, since that is what lives exactly
    //   as long as the entities do (Level::MountedMap). This only says what.
    // =============================================================================
    namespace HardReferences
    {
        /**
         * Every hard reference InData's entities name, deduplicated by (type, path). An empty path
         * is a real state — "no bullet yet" — and is skipped, never reported.
         */
        OPAAX_API TDynArray<HardReference> Collect(const MapData& InData, const ComponentRegistry& InRegistry);
    }
}
