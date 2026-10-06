#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;

    inline constexpr LogCategory LogMapSerializer{"MapSerializer"};

    // =============================================================================
    // MapSerializer — captures a World into MapData. Stateless.
    //   Separate named captures (not one with an optional filter), so "the whole world"
    //   is never asked for by accident.
    // =============================================================================
    class MapSerializer
    {
    public:
        /**
         * Every entity in InWorld (Play copy). Id is left invalid (not a map).
         * @param InRegistry Registered component types; unregistered ones are not written
         */
        static MapData CaptureWorld(const World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Only the entities of InMapId ("save this map"). Sets the result's Id.
         * Runtime-spawned entities never match.
         * @param InMapId An invalid id captures nothing (with a warning)
         * @return The captured data; empty when nothing matched
         */
        static MapData CaptureMap(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId);

        /**
         * Only the given entities, whatever their map (editor undo). Id is left invalid.
         * Invalid handles are skipped.
         */
        static MapData CaptureEntities(const World& InWorld, const ComponentRegistry& InRegistry,
                                       const TDynArray<EntityID>& InEntities);
    };
}
