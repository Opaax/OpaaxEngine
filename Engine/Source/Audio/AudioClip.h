#pragma once

#include "Audio/IAudioDecoder.h"
#include "Core/OpaaxTypes.h"
#include "Resources/Asset/Asset.h"

namespace Opaax
{
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
    // AudioClip — a sound file (.wav, .mp3, .flac) as an asset, read from the file itself: the raw
    //   sound with no playing settings (an AudioClipDef adds them). A short sound is decoded when it
    //   loads (it plays at no decoding cost); a long one (music, dialogue) keeps its file in memory and
    //   is decoded while it plays, so it costs its file size, not minutes of samples. A clip that did
    //   not load plays nothing.
    // =============================================================================
    class AudioClip final : public Asset
    {
        OPAAX_OBJECT(AudioClip, Asset)
        OPAAX_ASSET_SOURCE(".wav", ".mp3", ".flac")

        // =========================================================================
        // Types
        // =========================================================================
    public:
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

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        AudioClip();
        ~AudioClip() override;

        /**
         * Reads a sound from a file's bytes instead (generated sounds, packed data).
         * @param InNameForLogs Names the clip in the error when the bytes are not a supported sound
         * @return False when they are not (logged); the clip stays silent
         */
        bool LoadFromMemory(TDynArray<Uint8> InBytes, const char* InNameForLogs,
                            EResidency InResidency = EResidency::Auto);

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads the file and decodes it, or checks that it decodes (a streamed clip). */
        bool LoadSource(const char* InAbsPath) override;

        /** The decoded samples or the file's bytes. */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The samples or the file, shared with the voices playing it. Null for a silent clip. */
        const TSharedPtr<const AudioClipData>& GetData() const noexcept { return m_Data; }

        /** 1 or 2: a sound with more is mixed down to stereo. */
        Uint32 GetChannels()   const noexcept { return m_Channels; }
        Uint32 GetSampleRate() const noexcept { return m_SampleRate; }

        /** Samples per channel; 0 when the format does not tell. */
        Uint64 GetFrameCount() const noexcept { return m_FrameCount; }

        /** True when nothing loaded: nothing to play. */
        bool IsSilent() const noexcept { return m_Data == nullptr; }

        bool IsStreamed() const noexcept { return m_Data != nullptr && m_Data->IsStreamed(); }

        /** Seconds, or 0 when the length is unknown. */
        float GetDuration() const noexcept
        {
            return m_SampleRate > 0 ? static_cast<float>(static_cast<double>(m_FrameCount) / m_SampleRate) : 0.f;
        }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TSharedPtr<const AudioClipData> m_Data;

        Uint32 m_Channels   = 0;
        Uint32 m_SampleRate = 0;
        Uint64 m_FrameCount = 0;
    };
}
