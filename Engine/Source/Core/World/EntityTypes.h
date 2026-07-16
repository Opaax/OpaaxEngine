#pragma once

#include <entt/entt.hpp>

namespace Opaax
{
    // =============================================================================
    // Entity handle types — the runtime identity of an entity inside a World's
    //   entt::registry. EntityID is the raw handle; ENTITY_NONE is the null handle
    //   returned by lookups that resolve to nothing.
    // =============================================================================
    using EntityID = entt::entity;
    inline constexpr EntityID ENTITY_NONE = entt::null;
}
