#pragma once

#include <entt/entt.hpp>

#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // EntityID is the raw entt handle; ENTITY_NONE is the null handle.
    // =============================================================================
    using EntityID = entt::entity;
    inline constexpr EntityID ENTITY_NONE = entt::null;

    using EntityRegistry = entt::registry;

    // =============================================================================
    // MapId — which map an entity belongs to (a world has one registry; a map is a subset of it).
    //   Interned, and saved as its string. Invalid means runtime-spawned (never saved).
    // =============================================================================
    using MapId = OpaaxStringID;
}
