#pragma once

#include <nlohmann/json.hpp>

#include "Audio/AudioTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct AudioClipResource;

    // =============================================================================
    // AudioSourceComponent — a sound on an entity. In a Play world it starts with the entity when
    //   bPlayOnStart, follows the entity when spatial, and stops when the entity is destroyed.
    //   Gameplay replays or stops it through AudioSubsystem (or a behaviour's PlayAudioSource).
    // =============================================================================
    struct AudioSourceComponent
    {
        TResourcePath<AudioClipResource> Clip;

        EAudioBus Bus          = EAudioBus::Effects;
        float     Volume       = 1.f;
        float     Pitch        = 1.f;
        bool      bLoop        = false;
        bool      bPlayOnStart = true;

        /** Heard from the listener: panned, and quieter with distance. Off for music and UI. */
        bool  bSpatial    = true;
        float MinDistance = 100.f;    // full volume within, world units
        float MaxDistance = 1500.f;   // silent beyond

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioSourceComponent, Clip, Bus, Volume, Pitch, bLoop,
                                                    bPlayOnStart, bSpatial, MinDistance, MaxDistance)

        OPAAX_PROPERTIES(AudioSourceComponent,
                         OPAAX_PROP(Clip).SetTooltip("The sound file (.wav, .mp3, .flac)."),
                         OPAAX_PROP(Bus).SetTooltip("Mix group: its volume is set in the audio settings."),
                         OPAAX_PROP(Volume).SetRange(0.f, 2.f),
                         OPAAX_PROP(Pitch).SetRange(0.1f, 4.f).SetTooltip("2 is an octave up, and twice as fast."),
                         OPAAX_PROP(bLoop),
                         OPAAX_PROP(bPlayOnStart).SetTooltip("Plays when the entity starts in a Play world."),
                         OPAAX_PROP(bSpatial).SetTooltip("Heard from the listener: panned and attenuated.\n"
                                                          "Off for music and UI sounds."),
                         OPAAX_PROP(MinDistance).SetRange(0.f, 10000.f).SetTooltip("Full volume within this distance."),
                         OPAAX_PROP(MaxDistance).SetRange(0.f, 10000.f).SetTooltip("Silent beyond this distance."))
    };
}
