#pragma once

#include <nlohmann/json.hpp>

#include "Audio/AudioBusSettings.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringIDJson.h"
#include "Resources/Asset/AssetRef.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct AudioClipResource;

    // =============================================================================
    // AudioSourceComponent — a sound on an entity. In a Play world it starts with the entity when
    //   bPlayOnStart, follows the entity when spatial, and stops when the entity is destroyed
    //   (unless bStopOnDestroy is off: then it plays to its end).
    //   It plays a clip definition (Sound: variations, bus, spread...), or a sound file (Clip) with
    //   the settings below. Gameplay replays or stops it through AudioSubsystem (or a behaviour's
    //   PlayAudioSource).
    // =============================================================================
    struct AudioSourceComponent
    {
        /** A clip definition. When set, Clip and the settings below it are not used. */
        TAssetRef<AudioClipDef> Sound;

        TResourcePath<AudioClipResource> Clip;

        OpaaxStringID Bus          = AudioDefaultBus();
        float         Volume       = 1.f;   // times the definition's, with a Sound
        float         Pitch        = 1.f;   // times the definition's, with a Sound
        bool          bLoop        = false;
        bool          bPlayOnStart = true;

        /** Off: the sound plays to its end after the entity is gone (an explosion). */
        bool bStopOnDestroy = true;

        /** Heard from the listener: panned, and quieter with distance. Off for music and UI. */
        bool              bSpatial    = true;
        float             MinDistance = 100.f;    // full volume within, world units
        float             MaxDistance = 1500.f;   // silent beyond
        EAudioAttenuation Attenuation = EAudioAttenuation::Linear;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioSourceComponent, Sound, Clip, Bus, Volume, Pitch, bLoop,
                                                    bPlayOnStart, bStopOnDestroy, bSpatial, MinDistance, MaxDistance,
                                                    Attenuation)

        OPAAX_PROPERTIES(AudioSourceComponent,
                         OPAAX_PROP(Sound).SetTooltip("A clip definition: its clips, bus, volume, spread, "
                                                      "loop and space.\nWhen set, the fields below Clip are not used."),
                         OPAAX_PROP(Clip).SetTooltip("A sound file (.wav, .mp3, .flac), played with the settings below."),
                         OPAAX_PROP(Bus).SetTooltip("The bus it plays in (\"Music\", \"SFX\"...): Audio.Buses in Engine.config."),
                         OPAAX_PROP(Volume).SetRange(0.f, 2.f),
                         OPAAX_PROP(Pitch).SetRange(0.1f, 4.f).SetTooltip("2 is an octave up, and twice as fast."),
                         OPAAX_PROP(bLoop),
                         OPAAX_PROP(bPlayOnStart).SetTooltip("Plays when the entity starts in a Play world."),
                         OPAAX_PROP(bStopOnDestroy).SetTooltip("Off: the sound plays to its end after the entity is gone."),
                         OPAAX_PROP(bSpatial).SetTooltip("Heard from the listener: panned and attenuated.\n"
                                                          "Off for music and UI sounds."),
                         OPAAX_PROP(MinDistance).SetRange(0.f, 10000.f).SetTooltip("Full volume within this distance."),
                         OPAAX_PROP(MaxDistance).SetRange(0.f, 10000.f).SetTooltip("Silent beyond this distance."),
                         OPAAX_PROP(Attenuation))
    };
}
