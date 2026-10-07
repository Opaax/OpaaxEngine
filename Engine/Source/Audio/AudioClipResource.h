#pragma once

#include <optional>

#include "Core/OpaaxTypes.h"
#include "Resources/ResourceConcept.hpp"
#include "Resources/ResourceFormat.h"

namespace Opaax
{
    class LoadContext;

    // =============================================================================
    // AudioClipResource — a sound file, kept encoded in memory and decoded while it plays: a long
    //   music track costs its file size, not minutes of samples.
    //   Placeholder policy: a missing or unreadable file plays as silence.
    // =============================================================================
    struct AudioClipResource final
    {
        OPAAX_RESOURCE_FORMAT("Audio Clip", ".wav", ".mp3", ".flac")

        /** The file's bytes. Shared, so a sound still playing keeps them after the clip is unloaded. */
        TSharedPtr<const TDynArray<Uint8>> Encoded;

        Uint32 Channels   = 0;
        Uint32 SampleRate = 0;
        Uint64 FrameCount = 0;   // samples per channel; 0 when the format does not tell

        /** True for the placeholder: nothing to play. */
        bool IsSilent() const noexcept { return Encoded == nullptr; }

        /** Seconds, or 0 when the length is unknown. */
        float GetDuration() const noexcept
        {
            return SampleRate > 0 ? static_cast<float>(static_cast<double>(FrameCount) / SampleRate) : 0.f;
        }

        // ---- CResource contract -----------------------------------------------------------------
        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        /** Reads the file and checks that it decodes. Null for a missing or unsupported file (logged). */
        static std::optional<AudioClipResource> Load(const char* InPath, LoadContext& InCtx);

        /**
         * A clip from a file's bytes (generated sounds, packed data).
         * @param InNameForLogs Names the clip in the error when the bytes are not a supported sound
         */
        static std::optional<AudioClipResource> FromMemory(TDynArray<Uint8> InBytes, const char* InNameForLogs);

        static AudioClipResource Placeholder() { return AudioClipResource{}; }

        Uint64 ByteSize() const noexcept { return Encoded != nullptr ? static_cast<Uint64>(Encoded->size()) : 0; }
    };
}
