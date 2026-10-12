#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES

namespace Opaax
{
    // =============================================================================
    // EAudioBackend
    // =============================================================================
    /**
     * The library that opens the audio device and decodes sound files. The mixing is the engine's
     * own (AudioMixer), so every backend sounds the same. Only miniaudio for now.
     */
    enum class EAudioBackend : Uint8
    {
        Miniaudio
    };

    /** Name for logs and the value written in the config. */
    const char* ToString(EAudioBackend InBackend) noexcept;
}

OPAAX_ENUM_VALUES(Opaax::EAudioBackend, Miniaudio)
