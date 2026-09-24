#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/GUID/Guid.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // EntityMeta — per-entity identity stored in the World's registry: the stable Guid,
    //   a debug name, the Map that authored the entity, and the entity it hangs off. Added
    //   automatically by World::CreateEntity, so every entity is addressable by Guid and
    //   self-describing and Each<EntityMeta> is the complete all-entities view.
    //
    //   This is IDENTITY, not user data: it deliberately does NOT satisfy CComponent and
    //   must never be registered as an ordinary component. The snapshot core writes these
    //   fields by hand, because they are what an EntityData IS — a component entry
    //   round-tripping them would nest the entity's identity inside its own payload.
    //
    //   Parent is here rather than in a component for the same reason: a guid inside a
    //   component payload is copied verbatim by prefab instantiation and diffed against the
    //   raw template, so every child of every placement would carry a phantom override. The
    //   identity fields are the one place guids are already derived (§HR).
    // =============================================================================
    struct EntityMeta
    {
        Guid        Id;
        OpaaxString Name;
        MapId       OwnerMap; // invalid => runtime-spawned, never captured
        Guid        Parent;   // invalid => root; TransformComponent is LOCAL to this entity
    };
}
