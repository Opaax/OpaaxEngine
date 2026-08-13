#pragma once

#include <nlohmann/json.hpp>

#include "Core/Tag/OpaaxTagContainer.h"
#include "Core/Tag/OpaaxTagJson.h"

namespace Sandbox
{
    // =============================================================================
    // TagsComponent — the GAME's answer to "what is this entity?", and the first caller of
    //   OpaaxTag (I14). Tags are a VOCABULARY the engine provides; which entities carry which
    //   ones is content, so the component lives here beside HealthComponent rather than in the
    //   engine.
    //
    //   Cost of making a tag container serializable: the OpaaxTagJson.h include. The NLOHMANN
    //   macro is the same one every other component pays, and the engine names this type nowhere.
    // =============================================================================
    struct TagsComponent
    {
        Opaax::OpaaxTagContainer Tags;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(TagsComponent, Tags)
    };
}
