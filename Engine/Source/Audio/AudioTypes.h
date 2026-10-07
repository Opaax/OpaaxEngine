#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Audio);

    // =============================================================================
    // EAudioBus — the mix group a sound plays in. Each bus has its own volume (settings, options
    //   menu), under the master volume.
    // =============================================================================
    enum class EAudioBus : Uint8
    {
        Music,
        Effects,
        Ambience,
        UI
    };

    inline constexpr Uint8 AUDIO_BUS_COUNT = 4;

    /** Enum to string. */
    inline const char* ToString(const EAudioBus InBus) noexcept
    {
        switch (InBus)
        {
        case EAudioBus::Music:    return "Music";
        case EAudioBus::Effects:  return "Effects";
        case EAudioBus::Ambience: return "Ambience";
        case EAudioBus::UI:       return "UI";
        }

        return "Effects";
    }
}

OPAAX_ENUM_VALUES(Opaax::EAudioBus, Music, Effects, Ambience, UI)

namespace Opaax
{
    // =============================================================================
    // SoundHandle — one playing sound. Stays valid (and harmless) after the sound ends: every
    //   call on a finished sound does nothing.
    // =============================================================================
    struct SoundHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
        bool operator==(const SoundHandle& InOther) const noexcept { return Id == InOther.Id; }
    };

    // =============================================================================
    // PlaySoundParams — how a sound plays.
    // =============================================================================
    struct PlaySoundParams
    {
        EAudioBus Bus    = EAudioBus::Effects;
        float     Volume = 1.f;   // linear, 1 = as recorded
        float     Pitch  = 1.f;   // 2 = an octave up, and twice as fast
        bool      bLoop  = false;

        /** Positioned in the world and heard from the listener (panned, quieter with distance). */
        bool      bSpatial    = false;
        Vector2F  Position    = { 0.f, 0.f };
        float     MinDistance = 100.f;    // full volume within, world units
        float     MaxDistance = 1500.f;   // silent beyond
    };
}
