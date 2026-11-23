#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Config/TConfig.h"
#include "Engine/Tags/TagDefinition.h"

namespace Opaax
{
    /** The project's own gameplay tags, beside the ones code declares (OPAAX_REGISTER_TAG). */
    struct TagsConfigData
    {
        TDynArray<TagDefinition> Tags;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TagsConfigData, Tags)

        OPAAX_PROPERTIES(TagsConfigData,
                         OPAAX_PROP(Tags).SetTooltip("The project's tags, beside the ones code declares.\n"
                                                     "The pickers offer them; an edit applies at once."))
    };

    // =============================================================================
    // Config_Tags — <ProjectRoot>/Configs/Tags.config: the project's gameplay tags, like Unreal's
    //   DefaultGameplayTags.ini. The engine's TagRegistry follows it.
    // =============================================================================
    class Config_Tags final : public TConfig<TagsConfigData>
    {
        OPAAX_OBJECT(Config_Tags, Config)

    public:
        const char* FileName() const override { return "Tags.config"; }
    };
}
