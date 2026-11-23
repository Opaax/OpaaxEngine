#include "Editor/Properties/TagPicker.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Tags/Config_Tags.h"
#include "Engine/Tags/TagRegistry.h"

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(TagPicker);

    const TagRegistry* GetEditorTagRegistry()
    {
        return &OpaaxApplication::GetAppService<IEngine>().GetRegistries().Tags();
    }

    bool RegisterTagInConfig(const OpaaxTag InTag)
    {
        Config_Tags& lConfig = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Tags>();

        TagsConfigData lData = lConfig.GetProjectData();
        lData.Tags = WithTagListed(lData.Tags, InTag);

        // Through the config, as the Config panel edits it: the registry hears the change.
        OpaaxString lError;
        if (!lConfig.MergeText(TConfigCodec<TagsConfigData>::ToText(lData), lError) || !lConfig.Save())
        {
            OPAAX_LOG(LogTagPicker, Error, "'{}' could not be added to Tags.config: {}", InTag, lError.CStr());
            return false;
        }

        OPAAX_LOG(LogTagPicker, Info, "'{}' added to Tags.config", InTag);
        return true;
    }
}
