#pragma once

#include <nlohmann/json.hpp>

#include "Audio/AudioBank.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    // =============================================================================
    // AudioBanksComponent — audio banks a level needs: in a Play world, their sounds load when the
    //   entity appears (with its map) and are let go when it is destroyed or the world ends. Put it
    //   on one entity of the map that plays the sounds.
    // =============================================================================
    struct AudioBanksComponent
    {
        TDynArray<TAssetRef<AudioBank>> Banks;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioBanksComponent, Banks)

        OPAAX_PROPERTIES(AudioBanksComponent,
                         OPAAX_PROP(Banks).SetTooltip("Audio banks: their sounds are loaded before they play."))
    };
}
