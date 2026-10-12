#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // AudioFormat — what a sound holds: its channels, its sample rate and its length.
    // =============================================================================
    struct AudioFormat
    {
        Uint32 Channels   = 0;
        Uint32 SampleRate = 0;
        Uint64 FrameCount = 0;   // samples per channel; 0 when the format does not tell
    };

    // =============================================================================
    // IAudioDecoder — reads a sound file's bytes as float samples. Made by the audio backend
    //   (AudioAPI::OpenDecoder); the engine never sees the library that decodes.
    //   One thread at a time: a voice's decoder is read on the audio thread.
    // =============================================================================
    class IAudioDecoder
    {
    public:
        virtual ~IAudioDecoder() = default;

        /** Channels are 1 or 2: a sound with more is mixed down to stereo. */
        virtual const AudioFormat& GetFormat() const noexcept = 0;

        /**
         * Reads the next frames, interleaved (GetFormat().Channels floats per frame).
         * @return Frames read, maybe fewer than asked; 0 only at the end of the sound
         */
        virtual Uint64 Read(float* OutFrames, Uint64 InFrameCount) = 0;

        /** Goes back to the first frame. */
        virtual bool Rewind() = 0;
    };
}
