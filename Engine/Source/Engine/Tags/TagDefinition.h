#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Tag/OpaaxTag.h"
#include "Core/Tag/OpaaxTagJson.h"

namespace Opaax
{
    // =============================================================================
    // TagDefinition — one tag a project declares (Tags.config), with what it is for.
    // =============================================================================
    struct TagDefinition
    {
        OpaaxTag    Tag;
        OpaaxString Comment;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TagDefinition, Tag, Comment)

        OPAAX_PROPERTIES(TagDefinition,
                         OPAAX_PROP(Tag).SetTooltip("Dotted names, from the most general: \"UI.Layer.Popup\".\n"
                                                    "Its parents (\"UI\", \"UI.Layer\") exist with it."),
                         OPAAX_PROP(Comment).SetTooltip("What it is for, shown beside it in the pickers."))
    };
}
