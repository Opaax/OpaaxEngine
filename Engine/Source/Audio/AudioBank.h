#pragma once

#include <nlohmann/json.hpp>

#include "Audio/AudioClipDef.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/DataAsset/DataAssetRef.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct AudioClipResource;

    // =============================================================================
    // AudioBank — sounds to have loaded before they play: clip definitions (all their clips) and
    //   sound files. A data asset (.opaaxdata). A level lists its banks in AudioBanks components;
    //   Engine.config names one global bank, held for the whole game (menus, the player, UI).
    // =============================================================================
    struct AudioBank
    {
        TDynArray<TDataAssetRef<AudioClipDef>>      ClipDefs;
        TDynArray<TResourcePath<AudioClipResource>> Clips;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioBank, ClipDefs, Clips)

        OPAAX_PROPERTIES(AudioBank,
                         OPAAX_PROP(ClipDefs).SetTooltip("Clip definitions: every clip they can play is loaded."),
                         OPAAX_PROP(Clips).SetTooltip("Sound files played by path."))
    };
}
