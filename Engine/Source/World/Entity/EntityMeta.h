#pragma once

#include "Core/OpaaxString.hpp"
#include "Core/GUID/Guid.h"

namespace Opaax
{
    // =============================================================================
    // EntityMeta — per-entity identity stored in the World's registry: the stable
    //   Guid plus a debug name. Added automatically by World::CreateEntity, so every
    //   entity is addressable by Guid and self-describing.
    // =============================================================================
    struct EntityMeta
    {
        Guid        Id;
        OpaaxString Name;
    };
}
