#pragma once

#include "Audio/AudioClipDef.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    // =============================================================================
    // AudioBank — sounds to have loaded before they play: clip definitions (all their clips) and
    //   sound files. A level lists its banks in AudioBanks components; Engine.config names one global
    //   bank, held for the whole game (menus, the player, UI).
    // =============================================================================
    class AudioBank final : public Asset
    {
        OPAAX_OBJECT(AudioBank, Asset)

    public:
        TDynArray<TAssetRef<AudioClipDef>> ClipDefs;
        TDynArray<TAssetRef<AudioClip>>    Clips;

        OPAAX_PROPERTIES(AudioBank,
                         OPAAX_PROP(ClipDefs).SetTooltip("Clip definitions: every clip they can play is loaded."),
                         OPAAX_PROP(Clips).SetTooltip("Sound files played by path."))
    };
}
