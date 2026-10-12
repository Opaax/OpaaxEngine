#pragma once

#include <nlohmann/json.hpp>

#include "Audio/AudioBusSettings.h"
#include "Audio/AudioTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringIDJson.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct AudioClipResource;
    class AudioRandom;

    // =============================================================================
    // EAudioSelection — which of a clip definition's clips plays each time.
    // =============================================================================
    enum class EAudioSelection : Uint8
    {
        Single,           // always the first
        Random,           // any, by weight
        RandomNoRepeat,   // any but the last one played, by weight
        Sequential,       // in order, then again
        Shuffle           // each once in a random order, then a new order
    };

    /** Enum to string. */
    inline const char* ToString(const EAudioSelection InSelection) noexcept
    {
        switch (InSelection)
        {
        case EAudioSelection::Single:         return "Single";
        case EAudioSelection::Random:         return "Random";
        case EAudioSelection::RandomNoRepeat: return "RandomNoRepeat";
        case EAudioSelection::Sequential:     return "Sequential";
        case EAudioSelection::Shuffle:        return "Shuffle";
        }
        return "Random";
    }
}

OPAAX_ENUM_VALUES(Opaax::EAudioSelection, Single, Random, RandomNoRepeat, Sequential, Shuffle)

namespace Opaax
{
    // =============================================================================
    // AudioClipEntry — one variation of a clip definition.
    // =============================================================================
    struct AudioClipEntry
    {
        TResourcePath<AudioClipResource> Clip;

        /** How often it is picked by the random selections, against the others. */
        float Weight = 1.f;

        /** Times the definition's volume and pitch, for this clip only. */
        float VolumeScale = 1.f;
        float PitchScale  = 1.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioClipEntry, Clip, Weight, VolumeScale, PitchScale)

        OPAAX_PROPERTIES(AudioClipEntry,
                         OPAAX_PROP(Clip).SetTooltip("The sound file (.wav, .mp3, .flac)."),
                         OPAAX_PROP(Weight).SetRange(0.f, 100.f).SetTooltip("How often the random selections pick it."),
                         OPAAX_PROP(VolumeScale).SetRange(0.f, 2.f),
                         OPAAX_PROP(PitchScale).SetRange(0.1f, 4.f))
    };

    // =============================================================================
    // AudioClipDef — how a sound plays: its clips (variations, one chosen per play), its bus, volume
    //   and pitch with a random spread, looping, and where it is heard from. A data asset
    //   (.opaaxdata), written by designers; played by path like a sound file.
    // =============================================================================
    struct AudioClipDef
    {
        TDynArray<AudioClipEntry> Clips;
        EAudioSelection           Selection = EAudioSelection::Random;

        /** The bus it plays in ("Music", "SFX"...). */
        OpaaxStringID Bus = AudioDefaultBus();

        float Volume = 1.f;
        float Pitch  = 1.f;

        /** Each play moves the volume and pitch by up to this much, up or down (0.05 = 5%). */
        float VolumeSpread = 0.f;
        float PitchSpread  = 0.f;

        bool bLoop = false;

        /** Heard from the listener: panned, and quieter with distance. Off for music and UI. */
        bool              bSpatial    = false;
        float             MinDistance = 100.f;
        float             MaxDistance = 1500.f;
        EAudioAttenuation Attenuation = EAudioAttenuation::Linear;

        /** When every voice is busy, the lowest priority is replaced first. */
        Uint32 Priority = 128;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioClipDef, Clips, Selection, Bus, Volume, Pitch, VolumeSpread,
                                                    PitchSpread, bLoop, bSpatial, MinDistance, MaxDistance,
                                                    Attenuation, Priority)

        OPAAX_PROPERTIES(AudioClipDef,
                         OPAAX_PROP(Clips).SetTooltip("The variations: one is chosen each time it plays."),
                         OPAAX_PROP(Selection).SetTooltip("How the clip is chosen each time."),
                         OPAAX_PROP(Bus).SetTooltip("The bus it plays in: Audio.Buses in Engine.config."),
                         OPAAX_PROP(Volume).SetRange(0.f, 2.f),
                         OPAAX_PROP(Pitch).SetRange(0.1f, 4.f).SetTooltip("2 is an octave up, and twice as fast."),
                         OPAAX_PROP(VolumeSpread).SetRange(0.f, 1.f).SetTooltip("Random change of volume per play, up or down."),
                         OPAAX_PROP(PitchSpread).SetRange(0.f, 1.f).SetTooltip("Random change of pitch per play: 0.05 keeps a "
                                                                               "repeated sound from sounding robotic."),
                         OPAAX_PROP(bLoop),
                         OPAAX_PROP(bSpatial).SetTooltip("Heard from the listener: panned and attenuated.\n"
                                                          "Off for music and UI sounds."),
                         OPAAX_PROP(MinDistance).SetRange(0.f, 10000.f).SetTooltip("Full volume within this distance."),
                         OPAAX_PROP(MaxDistance).SetRange(0.f, 10000.f).SetTooltip("Silent beyond this distance."),
                         OPAAX_PROP(Attenuation),
                         OPAAX_PROP(Priority).SetRange(0.f, 255.f).SetTooltip("When every voice is busy, the lowest "
                                                                              "priority is replaced first."))
    };

    // =============================================================================
    // Choosing a variation — pure, so it is tested with a seeded random.
    // =============================================================================

    /** What a clip definition remembers between plays: the last clip, the next one, the shuffled order. */
    struct AudioSelectionState
    {
        Int32             Last = -1;
        Uint32            Next = 0;
        TDynArray<Uint32> Bag;
    };

    /**
     * The clip to play next from InDef.
     * @return Its index in InDef.Clips, or -1 when there is none
     */
    Int32 SelectAudioClip(const AudioClipDef& InDef, AudioSelectionState& InOutState, AudioRandom& InRandom);

    /** How clip InIndex of InDef plays: its bus, volume and pitch (spread drawn from InRandom), loop, space. */
    PlaySoundParams MakePlaySoundParams(const AudioClipDef& InDef, Int32 InIndex, AudioRandom& InRandom);
}
