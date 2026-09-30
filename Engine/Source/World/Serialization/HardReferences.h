#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class ComponentRegistry;
    struct MapData;

    /** One resource that must be loaded: type and asset path. */
    struct HardReference
    {
        Uint32      TypeId = 0;   // ResourceTypeID::Get<T>()
        OpaaxString Path;         // asset-relative or a mount
    };

    // =============================================================================
    // HardReferences — the resources a map's entities need loaded before they run
    //   (their THardResourcePath fields). Pure: reads entity data, not a world.
    // =============================================================================
    namespace HardReferences
    {
        /**
         * Every hard reference of InData's entities, without duplicates. Empty paths are skipped.
         */
        OPAAX_API TDynArray<HardReference> Collect(const MapData& InData, const ComponentRegistry& InRegistry);
    }
}
