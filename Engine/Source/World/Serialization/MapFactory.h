#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;

    inline constexpr LogCategory LogMapFactory{"MapFactory"};

    // =============================================================================
    // MapFactory — creates a World's entities from MapData. Inverse of MapSerializer. Stateless.
    // =============================================================================
    class MapFactory
    {
    public:
        /**
         * Creates every entity of InData in InWorld, keeping the guids (the only stable identity
         * for references). Additive: InWorld is not cleared.
         * An entity whose guid already exists is skipped; an unknown component is skipped with a warning.
         * @return Number of entities created
         */
        static Uint64 Instantiate(const MapData& InData, World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Makes InWorld match InData for the entities it names (used by undo):
         *   - recreates missing entities, with their guid;
         *   - overwrites identity and every component in the record;
         *   - removes registered components not in the record.
         * Entities not in InData are left alone. Bumps the world's revision if anything changed.
         * @return Number of entities restored
         */
        static Uint64 Restore(const MapData& InData, World& InWorld, const ComponentRegistry& InRegistry);
    };
}
