#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Tag/OpaaxTagContainer.h"
#include "Core/Tag/OpaaxTagJson.h"

namespace Opaax
{
    // =============================================================================
    // TagsComponent — which tags an entity carries ("Faction.Player", "Sandbox.Quad.White"). A query
    //   matches up the hierarchy: an entity tagged "Faction.Player" has "Faction". Saved as an array of
    //   strings, readable in the map; edited in the Inspector as a tag set.
    // =============================================================================
    struct TagsComponent
    {
        OpaaxTagContainer Tags;

        // _WITH_DEFAULT: a missing key keeps an empty container instead of throwing.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TagsComponent, Tags)

        OPAAX_PROPERTIES(TagsComponent, OPAAX_PROP(Tags))
    };
}
