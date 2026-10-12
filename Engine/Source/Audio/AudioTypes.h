#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Audio);

    // =============================================================================
    // EAudioAttenuation — how a spatial sound gets quieter between its min and max distances.
    // =============================================================================
    enum class EAudioAttenuation : Uint8
    {
        Linear,
        InverseSquare
    };

    /** Enum to string. */
    inline const char* ToString(const EAudioAttenuation InAttenuation) noexcept
    {
        switch (InAttenuation)
        {
        case EAudioAttenuation::Linear:        return "Linear";
        case EAudioAttenuation::InverseSquare: return "InverseSquare";
        }
        return "Linear";
    }
}

OPAAX_ENUM_VALUES(Opaax::EAudioAttenuation, Linear, InverseSquare)

namespace Opaax
{
    // =============================================================================
    // SoundHandle — one playing sound: a voice index and the generation of that voice. A voice
    //   reused by another sound has a new generation, so an old handle is recognised and every
    //   call on it does nothing.
    // =============================================================================
    struct SoundHandle
    {
        /** [index : 32 | generation : 32]. 0 is no sound: generation 0 is never used. */
        Uint64 Id = 0;

        static constexpr SoundHandle Make(const Uint32 InIndex, const Uint32 InGeneration) noexcept
        {
            return SoundHandle{ (static_cast<Uint64>(InIndex) << 32) | InGeneration };
        }

        constexpr Uint32 GetIndex()      const noexcept { return static_cast<Uint32>(Id >> 32); }
        constexpr Uint32 GetGeneration() const noexcept { return static_cast<Uint32>(Id & 0xFFFFFFFFu); }

        bool IsValid() const noexcept { return Id != 0; }
        bool operator==(const SoundHandle& InOther) const noexcept { return Id == InOther.Id; }
    };

    // =============================================================================
    // PlaySoundParams — how a sound plays.
    // =============================================================================
    struct PlaySoundParams
    {
        /** The bus it plays in, by name ("Music"). None: "SFX". */
        OpaaxStringID Bus;

        float Volume = 1.f;   // linear, 1 = as recorded
        float Pitch  = 1.f;   // 2 = an octave up, and twice as fast
        bool  bLoop  = false;

        /** Seconds to rise from silence; 0 starts at full volume. */
        float FadeInSeconds = 0.f;

        /** When every voice is busy, the lowest priority is replaced first (0 lowest, 255 highest). */
        Uint8 Priority = 128;

        /** Positioned in the world and heard from the listener (panned, quieter with distance). */
        bool              bSpatial    = false;
        Vector2F          Position    = { 0.f, 0.f };
        float             MinDistance = 100.f;    // full volume within, world units
        float             MaxDistance = 1500.f;   // silent beyond
        EAudioAttenuation Attenuation = EAudioAttenuation::Linear;
    };
}
