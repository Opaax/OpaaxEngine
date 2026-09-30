#pragma once

#include <nlohmann/json.hpp>

#include "Core/Tag/OpaaxTagContainer.h"
#include "Core/Tag/OpaaxTagJson.h"

namespace Sandbox
{
    // =============================================================================
    // TagsComponent — which tags an entity carries. OpaaxTag is the engine's vocabulary; which entity
    //   has which tag is game content, so the component lives here. Serializable through
    //   OpaaxTagJson.h.
    // =============================================================================
    struct TagsComponent
    {
        Opaax::OpaaxTagContainer Tags;

        // _WITH_DEFAULT: a missing key keeps an empty container instead of throwing.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TagsComponent, Tags)
    };
}
