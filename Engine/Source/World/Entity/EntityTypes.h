#pragma once

#include <entt/entt.hpp>

#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // Entity handle types — the runtime identity of an entity inside a World's
    //   entt::registry. EntityID is the raw handle; ENTITY_NONE is the null handle
    //   returned by lookups that resolve to nothing.
    // =============================================================================
    using EntityID = entt::entity;
    inline constexpr EntityID ENTITY_NONE = entt::null;

    using EntityRegistry = entt::registry;

    // =============================================================================
    // MapId — which Map an entity was authored into. A World owns ONE registry, so a
    //   Map is not a separate container: it is a PARTITION of that registry, and this
    //   tag is what makes the partition addressable. Capturing or unloading a map is a
    //   filter over this value.
    //
    //   An interned OpaaxStringID (I2-safe out-of-line pool, same reasoning as F4b's
    //   DebugChannel): 4 bytes per entity, comparison is an integer compare. It is
    //   derived from the map's authoring name, so it SERIALIZES AS THE STRING — an
    //   interned id is not stable across runs.
    //
    //   The default (invalid) value means RUNTIME-SPAWNED: a bullet, a particle, anything
    //   created during play that no map authored. Filtered capture never writes those out.
    //
    //   NOTE: lives here because EntityMeta is its only consumer today; expect it to move
    //   to the Map system when that lands (M5).
    // =============================================================================
    using MapId = OpaaxStringID;
}
