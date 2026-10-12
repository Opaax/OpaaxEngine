#pragma once

#include "Audio/Mixer/AudioCommands.h"
#include "Audio/Mixer/AudioFader.h"
#include "Audio/Mixer/SpscQueue.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    /** The mixer's output is always stereo: the backend's device converts to what it plays. */
    inline constexpr Uint32 AUDIO_OUTPUT_CHANNELS = 2;

    // =============================================================================
    // AudioBusDesc — one bus of the mix tree.
    // =============================================================================
    struct AudioBusDesc
    {
        OpaaxStringID Id;
        Int32         Parent = -1;    // index of the parent bus, before this one; -1 for the master
        float         Volume = 1.f;
    };

    // =============================================================================
    // AudioMixer — mixes every voice into stereo frames: reads each voice's source (resampled
    //   for its pitch and sample rate), applies its volume, fade, bus chain, distance and pan, and
    //   adds it in. Engine code, independent of the audio library: the backend only plays what it
    //   makes.
    //
    //   Single-threaded: Apply and Mix are called by one thread (the audio thread, or the game
    //   thread in a headless run). Never allocates after construction. Voices are addressed by
    //   the handle the game thread reserved (index + generation); a command for an old handle does
    //   nothing. Every gain change is ramped over one block, so nothing clicks.
    // =============================================================================
    class AudioMixer
    {
        // =============================================================================
        // Types
        // =============================================================================
    public:
        struct Desc
        {
            Uint32 SampleRate  = 48000;
            Uint32 MaxVoices   = 64;
            Uint32 BlockFrames = 256;

            /** The bus tree, master first. Empty: a master alone. */
            TDynArray<AudioBusDesc> Buses;
        };

        /** A voice replaced while it is heard fades out in one of these for one block. */
        static constexpr Uint32 TAIL_VOICES = 8;

        /** Source frames read per output frame at most (pitch x sample rate ratio). */
        static constexpr double MAX_STEP = 8.0;

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /** @param InEvents Where VoiceEnded is reported; holds at least 2 x (MaxVoices + TAIL_VOICES) */
        AudioMixer(const Desc& InDesc, TSpscQueue<AudioEvent>& InEvents);

        AudioMixer(const AudioMixer&)            = delete;
        AudioMixer& operator=(const AudioMixer&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void Apply(const AudioCommand& InCommand);

        /** Mixes InFrameCount stereo frames into OutFrames (overwritten), block by block. */
        void Mix(float* OutFrames, Uint32 InFrameCount);

        /** Frames an AudioStreamReader must keep decoded for this mixer's blocks. */
        static Uint32 GetStreamWindowFrames(Uint32 InBlockFrames) noexcept;

        Uint32 GetSampleRate()  const noexcept { return m_SampleRate; }
        Uint32 GetBlockFrames() const noexcept { return m_BlockFrames; }
        Uint32 GetMaxVoices()   const noexcept { return static_cast<Uint32>(m_Voices.size()); }
        Uint32 GetBusCount()    const noexcept { return static_cast<Uint32>(m_Buses.size()); }

        /** Voices in use, tails apart. */
        Uint32 GetActiveVoiceCount() const noexcept;

        /** VoiceEnded events lost to a full queue (the queue is too small). */
        Uint32 GetDroppedEventCount() const noexcept { return m_DroppedEvents; }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        struct Voice
        {
            SoundHandle    Handle;   // invalid: the voice is free
            AudioVoiceDesc Desc;
            AudioFader     Fader;
            double         Cursor    = 0.0;   // source frame, with its fraction
            float          LastLeft  = 0.f;   // gains reached at the end of the last block
            float          LastRight = 0.f;
            bool           bStarted  = false;
            bool           bStopping = false;
            bool           bPaused   = false;

            bool IsFree() const noexcept { return !Handle.IsValid(); }
        };

        struct Bus
        {
            AudioBusDesc Desc;
            AudioFader   Fader;
            bool         bMute = false;
            bool         bSolo = false;
        };

        Voice* Find(SoundHandle InHandle) noexcept;
        void   Start(const AudioCommand& InCommand);
        void   MoveToTail(Voice& InVoice);
        void   End(Voice& InVoice);

        void MixBlock(float* OutFrames, Uint32 InFrames);
        void UpdateBusGains(Uint32 InFrames);

        /** Mixes one block of InVoice into OutFrames. @return True when the voice is done */
        bool MixVoice(Voice& InVoice, float* OutFrames, Uint32 InFrames);

        /** Copies source frames [InFirst, InFirst + InCount) into m_Source. @return Frames before the end */
        Uint32 FetchSource(const Voice& InVoice, Uint64 InFirst, Uint32 InCount);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_SampleRate  = 48000;
        Uint32 m_BlockFrames = 256;

        TDynArray<Voice>               m_Voices;   // indexed by handle index
        TFixedArray<Voice, TAIL_VOICES> m_Tails;

        TDynArray<Bus>   m_Buses;
        TDynArray<float> m_BusGains;   // this block's gain of each bus, chain, mute and solo applied
        TDynArray<Uint8> m_BusSoloed;  // the bus or one above it is soloed

        TDynArray<float> m_Source;     // one voice's source frames for a block
        Vector2F         m_Listener = { 0.f, 0.f };

        TSpscQueue<AudioEvent>& m_Events;
        Uint32 m_DroppedEvents = 0;
    };
}
