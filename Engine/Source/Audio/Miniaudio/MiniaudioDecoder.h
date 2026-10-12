#pragma once

#include "Audio/IAudioDecoder.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // MiniaudioDecoder — IAudioDecoder over miniaudio's decoders (.wav, .mp3, .flac), reading the
    //   file's bytes in memory.
    // =============================================================================
    class MiniaudioDecoder final : public IAudioDecoder
    {
    public:
        /** Null when the bytes are not a sound miniaudio decodes. */
        static TUniquePtr<IAudioDecoder> Open(const Uint8* InBytes, Uint64 InSize);

        ~MiniaudioDecoder() override;

        MiniaudioDecoder(const MiniaudioDecoder&)            = delete;
        MiniaudioDecoder& operator=(const MiniaudioDecoder&) = delete;

        //~Begin IAudioDecoder interface
        const AudioFormat& GetFormat() const noexcept override { return m_Format; }
        Uint64 Read(float* OutFrames, Uint64 InFrameCount) override;
        bool   Rewind() override;
        //~End IAudioDecoder interface

    private:
        MiniaudioDecoder();

        struct Impl;
        TUniquePtr<Impl> m_Impl;
        AudioFormat      m_Format;
    };
}
