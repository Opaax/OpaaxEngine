#pragma once

#include "Audio/AudioBackend.h"
#include "Audio/IAudioDecoder.h"
#include "Audio/IAudioDevice.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // AudioAPI — creates the backend's device and decoders (implemented in AudioBackendFactory.cpp,
    //   the only file that includes a backend). Like the RHI: switching library is one more case
    //   there, nothing else changes.
    // =============================================================================
    class AudioAPI
    {
        // =============================================================================
        // Creation
        // =============================================================================
    public:
        /** The output device of InBackend. Null if the backend is not available (logged). */
        static TUniquePtr<IAudioDevice> CreateDevice(EAudioBackend InBackend);

        /**
         * A decoder over a sound file's bytes, with the backend in use. The bytes must outlive it.
         * @return Null when the bytes are not a supported sound
         */
        static TUniquePtr<IAudioDecoder> OpenDecoder(const Uint8* InBytes, Uint64 InSize);

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        /** The backend decoders are opened with. Set by the AudioManager when it starts. */
        static EAudioBackend GetBackend() noexcept { return s_Backend; }
        static void          SetBackend(EAudioBackend InBackend) noexcept { s_Backend = InBackend; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Defined in the .cpp, so there is one value for the whole program. */
        static EAudioBackend s_Backend;
    };
}
