#pragma once

#include "Audio/IAudioDecoder.h"
#include "Core/OpaaxTypes.h"
#include "Resources/ResourceConcept.hpp"
#include "Resources/ResourceFormat.h"

namespace Opaax
{
    class LoadContext;

    // =============================================================================
    // AudioClipData — a sound's samples or its file, immutable and shared: a voice keeps it alive
    //   while it plays, even after the clip is unloaded.
    // =============================================================================
    struct AudioClipData
    {
        AudioFormat Format;

        /** Decoded: the whole sound, interleaved (Format.Channels floats per frame). */
        TDynArray<float> Frames;

        /** Streamed: the file's bytes, decoded while the sound plays. */
        TDynArray<Uint8> Encoded;

        bool IsStreamed() const noexcept { return !Encoded.empty(); }
    };

    // =============================================================================
    // AudioClipResource — a sound file (.wav, .mp3, .flac), the raw sound with no playing settings.
    //   A short sound is decoded when it loads (it plays at no decoding cost); a long one (music,
    //   dialogue) keeps its file in memory and is decoded while it plays, so it costs its file size,
    //   not minutes of samples.
    //   Placeholder policy: a missing or unreadable file plays as silence.
    // =============================================================================
    struct AudioClipResource final
    {
        OPAAX_RESOURCE_FORMAT("Audio Clip", ".wav", ".mp3", ".flac")

        /** Longer sounds are streamed. */
        static constexpr float  DECODE_MAX_SECONDS = 10.f;

        /** Sounds that would take more decoded are streamed. */
        static constexpr Uint64 DECODE_MAX_BYTES = 2ull * 1024 * 1024;

        /** How the clip is kept: decided from its length, or forced (tests, tools). */
        enum class EResidency : Uint8
        {
            Auto,
            Decoded,
            Streamed
        };

        TSharedPtr<const AudioClipData> Data;

        Uint32 Channels   = 0;   // 1 or 2: a sound with more is mixed down to stereo
        Uint32 SampleRate = 0;
        Uint64 FrameCount = 0;   // samples per channel; 0 when the format does not tell

        /** True for the placeholder: nothing to play. */
        bool IsSilent() const noexcept { return Data == nullptr; }

        bool IsStreamed() const noexcept { return Data != nullptr && Data->IsStreamed(); }

        /** Seconds, or 0 when the length is unknown. */
        float GetDuration() const noexcept
        {
            return SampleRate > 0 ? static_cast<float>(static_cast<double>(FrameCount) / SampleRate) : 0.f;
        }

        // ---- CResource contract -----------------------------------------------------------------
        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        /** Reads the file and decodes it or checks that it decodes. Null for a missing or unsupported file (logged). */
        static TOptional<AudioClipResource> Load(const char* InPath, LoadContext& InCtx);

        /**
         * A clip from a file's bytes (generated sounds, packed data).
         * @param InNameForLogs Names the clip in the error when the bytes are not a supported sound
         */
        static TOptional<AudioClipResource> FromMemory(TDynArray<Uint8> InBytes, const char* InNameForLogs,
                                                       EResidency InResidency = EResidency::Auto);

        static AudioClipResource Placeholder() { return AudioClipResource{}; }

        /** Memory held: the decoded samples or the file's bytes. */
        Uint64 ByteSize() const noexcept
        {
            if (Data == nullptr) { return 0; }
            return Data->IsStreamed() ? static_cast<Uint64>(Data->Encoded.size())
                                      : static_cast<Uint64>(Data->Frames.size() * sizeof(float));
        }
    };
}
