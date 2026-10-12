#include "Audio/AudioAPI.h"

#include "Audio/AudioTypes.h"
#include "Audio/Miniaudio/MiniaudioDecoder.h"
#include "Audio/Miniaudio/MiniaudioDevice.h"

namespace Opaax
{
    EAudioBackend AudioAPI::s_Backend = EAudioBackend::Miniaudio;

    const char* ToString(const EAudioBackend InBackend) noexcept
    {
        switch (InBackend)
        {
            case EAudioBackend::Miniaudio: return "Miniaudio";
        }
        return "Unknown";
    }

    TUniquePtr<IAudioDevice> AudioAPI::CreateDevice(const EAudioBackend InBackend)
    {
        switch (InBackend)
        {
            case EAudioBackend::Miniaudio: return MakeUnique<MiniaudioDevice>();
        }

        OPAAX_LOG(LogAudio, Error, "Audio backend '{}' is not available", ToString(InBackend));
        return nullptr;
    }

    TUniquePtr<IAudioDecoder> AudioAPI::OpenDecoder(const Uint8* InBytes, const Uint64 InSize)
    {
        switch (s_Backend)
        {
            case EAudioBackend::Miniaudio: return MiniaudioDecoder::Open(InBytes, InSize);
        }
        return nullptr;
    }
}
