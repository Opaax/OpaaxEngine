#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;

    inline constexpr LogCategory LogMapSerializer{"MapSerializer"};

    // =============================================================================
    // MapSerializer — the CAPTURE half of the snapshot core: a live World becomes MapData.
    //
    //   Stateless (every member static, no instance): it is a transformation, and holding
    //   state here would be a second place for the truth to live.
    // =============================================================================
    class OPAAX_API MapSerializer
    {
    public:
        /**
         * Walk InWorld's entities and serialize every component the registry knows about.
         *
         * @param InWorld    Read-only — capture never mutates the world it reads.
         * @param InRegistry Decides what is serializable. A component type absent from it is
         *                   simply not written: an unregistered type has no stable name to
         *                   write under, and inventing one would produce a map nothing can load.
         * @param InFilter   Invalid (the default) captures the WHOLE world — which is what
         *                   PIE's world clone needs. A valid MapId narrows to the entities
         *                   that map authored, which is what "save this map" needs. Because a
         *                   runtime-spawned entity carries an invalid OwnerMap, it can never
         *                   match a valid filter — excluding bullets and VFX from a saved map
         *                   falls out of the rule rather than needing a special case.
         * @return The captured data. Empty when nothing matched.
         */
        static MapData Capture(const World& InWorld, const ComponentRegistry& InRegistry, MapId InFilter = {});
    };
}
