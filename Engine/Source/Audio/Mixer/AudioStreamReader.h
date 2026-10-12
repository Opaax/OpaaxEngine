#pragma once

#include "Audio/IAudioDecoder.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // AudioStreamReader — a sound decoded while it plays (music, long ambiences): a window of
    //   decoded frames ahead of the voice, refilled from a decoder. Made on the game thread when the
    //   sound starts (its memory is allocated then), read only by the mixer while the voice lives.
    // =============================================================================
    class AudioStreamReader
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /**
         * @param InWindowFrames Frames kept decoded: at least what one mix block reads
         *   (AudioMixer::GetStreamWindowFrames)
         */
        AudioStreamReader(TUniquePtr<IAudioDecoder> InDecoder, Uint32 InWindowFrames);

        AudioStreamReader(const AudioStreamReader&)            = delete;
        AudioStreamReader& operator=(const AudioStreamReader&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Copies frames [InFirst, InFirst + InCount) of the stream into OutFrames (interleaved),
         * decoding what is missing. A looping stream starts again after its last frame, so stream
         * frames keep counting up; past the end of one that does not loop, frames are silence.
         * InFirst never goes back between calls.
         * @return Frames copied before the end (InCount while the stream goes on)
         */
        Uint32 Fetch(Uint64 InFirst, Uint32 InCount, bool bInLoop, float* OutFrames);

        Uint32 GetChannels()   const noexcept { return m_Channels; }
        Uint32 GetSampleRate() const noexcept { return m_SampleRate; }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** Decodes until the window reaches stream frame InEnd, or the stream ends. */
        void FillTo(Uint64 InEnd, bool bInLoop);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IAudioDecoder> m_Decoder;
        Uint32 m_Channels   = 0;
        Uint32 m_SampleRate = 0;

        TDynArray<float> m_Window;
        Uint32 m_WindowCapacity = 0;   // frames
        Uint32 m_WindowCount    = 0;   // frames decoded and kept
        Uint64 m_WindowStart    = 0;   // stream frame of the window's first frame

        bool m_bEnded = false;   // the last frame was decoded and the stream does not loop
    };
}
