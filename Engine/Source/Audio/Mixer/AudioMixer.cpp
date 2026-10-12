#include "Audio/Mixer/AudioMixer.h"

#include <algorithm>
#include "Core/Maths/Maths.h"
#include <cstring>

#include "Audio/Mixer/AudioSpatial.h"
#include "Audio/Mixer/AudioStreamReader.h"

namespace Opaax
{
    namespace
    {
        constexpr float MIN_PITCH = 0.01f;
        constexpr float MAX_PITCH = 4.f;

        /** Frames one block may read: the block at the fastest step, the fraction and the next frame. */
        Uint32 SourceFramesPerBlock(const Uint32 InBlockFrames) noexcept
        {
            return static_cast<Uint32>(Maths::Ceil(InBlockFrames * AudioMixer::MAX_STEP)) + 4;
        }
    }

    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    AudioMixer::AudioMixer(const Desc& InDesc, TSpscQueue<AudioEvent>& InEvents)
        : m_SampleRate(Maths::Max(InDesc.SampleRate, 1u))
        , m_BlockFrames(Maths::Max(InDesc.BlockFrames, 1u))
        , m_Voices(Maths::Max(InDesc.MaxVoices, 1u))
        , m_Events(InEvents)
    {
        // The master first; a parent must come before its children, else it is the master.
        m_Buses.reserve(Maths::Max<size_t>(InDesc.Buses.size(), 1));
        for (const AudioBusDesc& lDesc : InDesc.Buses)
        {
            Bus lBus;
            lBus.Desc        = lDesc;
            lBus.Desc.Volume = Maths::Max(lDesc.Volume, 0.f);
            lBus.Desc.Parent = m_Buses.empty() ? -1 : Maths::Clamp<Int32>(lDesc.Parent, 0, static_cast<Int32>(m_Buses.size()) - 1);
            m_Buses.push_back(lBus);
        }

        if (m_Buses.empty())
        {
            Bus lMaster;
            lMaster.Desc.Id = OPAAX_ID("Master");
            m_Buses.push_back(lMaster);
        }

        m_BusGains.resize(m_Buses.size(), 1.f);
        m_BusSoloed.resize(m_Buses.size(), 0);
        m_Source.resize(static_cast<size_t>(SourceFramesPerBlock(m_BlockFrames)) * AUDIO_OUTPUT_CHANNELS, 0.f);
    }

    // =========================================================================
    // Functions
    // =========================================================================
    void AudioMixer::Apply(const AudioCommand& InCommand)
    {
        switch (InCommand.Type)
        {
        case EAudioCommand::Play:
            Start(InCommand);
            return;

        case EAudioCommand::StopAll:
            for (Voice& lVoice : m_Voices)
            {
                if (!lVoice.IsFree())
                {
                    lVoice.bStopping = true;
                    lVoice.Fader.FadeTo(0.f, InCommand.Frames);
                }
            }
            return;

        case EAudioCommand::SetListener:
            m_Listener = InCommand.Position;
            return;

        case EAudioCommand::SetBusVolume:
        case EAudioCommand::FadeBus:
        case EAudioCommand::SetBusMute:
        case EAudioCommand::SetBusSolo:
        {
            if (InCommand.Bus >= m_Buses.size())
            {
                return;
            }

            Bus& lBus = m_Buses[InCommand.Bus];
            if      (InCommand.Type == EAudioCommand::SetBusVolume) { lBus.Desc.Volume = Maths::Max(InCommand.Value, 0.f); }
            else if (InCommand.Type == EAudioCommand::FadeBus)      { lBus.Fader.FadeTo(Maths::Max(InCommand.Value, 0.f), InCommand.Frames); }
            else if (InCommand.Type == EAudioCommand::SetBusMute)   { lBus.bMute = InCommand.bFlag; }
            else                                                    { lBus.bSolo = InCommand.bFlag; }
            return;
        }

        default:
            break;
        }

        // The rest target one voice: an old handle finds nothing.
        Voice* lVoice = Find(InCommand.Handle);
        if (lVoice == nullptr)
        {
            return;
        }

        switch (InCommand.Type)
        {
        case EAudioCommand::Stop:
            lVoice->bStopping = true;
            lVoice->Fader.FadeTo(0.f, InCommand.Frames);
            break;

        case EAudioCommand::SetPaused:
            lVoice->bPaused = InCommand.bFlag;
            break;

        case EAudioCommand::SetVolume:
            lVoice->Desc.Volume = Maths::Max(InCommand.Value, 0.f);
            break;

        case EAudioCommand::FadeVolume:
            // A stopping voice keeps fading out.
            if (!lVoice->bStopping)
            {
                lVoice->Fader.FadeTo(Maths::Max(InCommand.Value, 0.f), InCommand.Frames);
            }
            break;

        case EAudioCommand::SetPitch:
            lVoice->Desc.Pitch = Maths::Clamp(InCommand.Value, MIN_PITCH, MAX_PITCH);
            break;

        case EAudioCommand::SetPosition:
            lVoice->Desc.Position = InCommand.Position;
            break;

        default:
            break;
        }
    }

    void AudioMixer::Mix(float* OutFrames, const Uint32 InFrameCount)
    {
        Uint32 lDone = 0;
        while (lDone < InFrameCount)
        {
            const Uint32 lFrames = Maths::Min(m_BlockFrames, InFrameCount - lDone);
            MixBlock(OutFrames + static_cast<size_t>(lDone) * AUDIO_OUTPUT_CHANNELS, lFrames);
            lDone += lFrames;
        }
    }

    Uint32 AudioMixer::GetStreamWindowFrames(const Uint32 InBlockFrames) noexcept
    {
        return SourceFramesPerBlock(Maths::Max(InBlockFrames, 1u));
    }

    Uint32 AudioMixer::GetActiveVoiceCount() const noexcept
    {
        return static_cast<Uint32>(std::count_if(m_Voices.begin(), m_Voices.end(),
                                                 [](const Voice& InVoice) { return !InVoice.IsFree(); }));
    }

    // =========================================================================
    // Internal — voices
    // =========================================================================
    AudioMixer::Voice* AudioMixer::Find(const SoundHandle InHandle) noexcept
    {
        const Uint32 lIndex = InHandle.GetIndex();
        if (!InHandle.IsValid() || lIndex >= m_Voices.size() || m_Voices[lIndex].Handle != InHandle)
        {
            return nullptr;
        }
        return &m_Voices[lIndex];
    }

    void AudioMixer::Start(const AudioCommand& InCommand)
    {
        const AudioVoiceDesc& lDesc  = InCommand.Voice;
        const Uint32          lIndex = InCommand.Handle.GetIndex();

        // Nothing to play: the voice ends at once, so the game thread frees what it holds.
        const bool bPlayable = InCommand.Handle.IsValid() && lIndex < m_Voices.size() && !lDesc.Source.IsEmpty()
                               && (lDesc.Source.Channels == 1 || lDesc.Source.Channels == 2) && lDesc.Source.SampleRate > 0;
        if (!bPlayable)
        {
            Voice lNothing;
            lNothing.Handle = InCommand.Handle;
            End(lNothing);
            return;
        }

        // The game thread reuses an index only to replace its sound: the old one fades out apart.
        Voice& lVoice = m_Voices[lIndex];
        if (!lVoice.IsFree())
        {
            MoveToTail(lVoice);
        }

        lVoice             = Voice{};
        lVoice.Handle      = InCommand.Handle;
        lVoice.Desc        = lDesc;
        lVoice.Desc.Volume = Maths::Max(lDesc.Volume, 0.f);
        lVoice.Desc.Pitch  = Maths::Clamp(lDesc.Pitch, MIN_PITCH, MAX_PITCH);
        lVoice.Desc.Bus    = lDesc.Bus < m_Buses.size() ? lDesc.Bus : 0;

        if (lDesc.FadeInFrames > 0)
        {
            lVoice.Fader.Set(0.f);
            lVoice.Fader.FadeTo(1.f, lDesc.FadeInFrames);
        }
    }

    void AudioMixer::MoveToTail(Voice& InVoice)
    {
        for (Voice& lTail : m_Tails)
        {
            if (lTail.IsFree())
            {
                lTail           = InVoice;
                lTail.bStopping = true;
                lTail.Fader.Set(0.f);
                InVoice.Handle  = SoundHandle{};
                return;
            }
        }

        // No tail free: cut at once (it may click).
        End(InVoice);
    }

    void AudioMixer::End(Voice& InVoice)
    {
        if (!m_Events.Push(AudioEvent{ EAudioEvent::VoiceEnded, InVoice.Handle }))
        {
            ++m_DroppedEvents;
        }

        InVoice.Handle = SoundHandle{};
        InVoice.Desc.Source = AudioVoiceSource{};
    }

    // =========================================================================
    // Internal — mixing
    // =========================================================================
    void AudioMixer::MixBlock(float* OutFrames, const Uint32 InFrames)
    {
        std::memset(OutFrames, 0, sizeof(float) * InFrames * AUDIO_OUTPUT_CHANNELS);
        UpdateBusGains(InFrames);

        for (Voice& lVoice : m_Voices)
        {
            if (!lVoice.IsFree() && MixVoice(lVoice, OutFrames, InFrames))
            {
                End(lVoice);
            }
        }

        for (Voice& lTail : m_Tails)
        {
            if (!lTail.IsFree() && MixVoice(lTail, OutFrames, InFrames))
            {
                End(lTail);
            }
        }
    }

    void AudioMixer::UpdateBusGains(const Uint32 InFrames)
    {
        bool bAnySolo = false;

        for (size_t lIndex = 0; lIndex < m_Buses.size(); ++lIndex)
        {
            Bus& lBus = m_Buses[lIndex];
            lBus.Fader.Tick(InFrames);

            const Int32 lParent      = lBus.Desc.Parent;
            const float lParentGain  = lParent >= 0 ? m_BusGains[lParent] : 1.f;
            const bool  bParentSolo  = lParent >= 0 && m_BusSoloed[lParent] != 0;

            m_BusGains[lIndex]  = lBus.bMute ? 0.f : lParentGain * lBus.Desc.Volume * lBus.Fader.Current;
            m_BusSoloed[lIndex] = (lBus.bSolo || bParentSolo) ? 1 : 0;
            bAnySolo            = bAnySolo || lBus.bSolo;
        }

        // While a bus is soloed, only it and the buses under it are heard.
        if (bAnySolo)
        {
            for (size_t lIndex = 0; lIndex < m_Buses.size(); ++lIndex)
            {
                if (m_BusSoloed[lIndex] == 0)
                {
                    m_BusGains[lIndex] = 0.f;
                }
            }
        }
    }

    bool AudioMixer::MixVoice(Voice& InVoice, float* OutFrames, const Uint32 InFrames)
    {
        const AudioVoiceDesc& lDesc = InVoice.Desc;
        InVoice.Fader.Tick(InFrames);

        // The gains to reach by the end of this block.
        float lGain = lDesc.Volume * InVoice.Fader.Current * m_BusGains[lDesc.Bus];
        float lPan  = lDesc.Pan;
        if (lDesc.bSpatial)
        {
            const Vector2F lOffset = lDesc.Position - m_Listener;
            lGain *= AudioSpatial::Attenuate(lDesc.Attenuation, Maths::Sqrt(lOffset.x * lOffset.x + lOffset.y * lOffset.y),
                                             lDesc.MinDistance, lDesc.MaxDistance);
            lPan   = AudioSpatial::PanFromOffset(lOffset.x, lDesc.MaxDistance);
        }

        if (InVoice.bPaused)
        {
            lGain = 0.f;
        }

        const Vector2F lBalance = AudioSpatial::BalanceGains(lPan);
        const float    lLeft    = lGain * lBalance.x;
        const float    lRight   = lGain * lBalance.y;

        // A sound starts at its gain (its attack is its own), unless it fades in.
        if (!InVoice.bStarted)
        {
            InVoice.bStarted  = true;
            InVoice.LastLeft  = lDesc.FadeInFrames > 0 ? 0.f : lLeft;
            InVoice.LastRight = lDesc.FadeInFrames > 0 ? 0.f : lRight;
        }

        // Paused and already silent: it keeps its place without being read.
        if (InVoice.bPaused && InVoice.LastLeft == 0.f && InVoice.LastRight == 0.f)
        {
            return InVoice.bStopping;
        }

        // Read the source frames this block covers.
        const double lStep  = Maths::Clamp(static_cast<double>(lDesc.Pitch) * lDesc.Source.SampleRate / m_SampleRate,
                                         1.0 / 1024.0, MAX_STEP);
        const double lFloor = Maths::Floor(InVoice.Cursor);
        const Uint64 lFirst = static_cast<Uint64>(lFloor);
        const Uint32 lCount = static_cast<Uint32>((InVoice.Cursor - lFloor) + InFrames * lStep) + 2;
        const Uint32 lValid = FetchSource(InVoice, lFirst, lCount);

        // Resample linearly, ramp the gains across the block, add into the mix.
        const float* lSource = m_Source.data();
        const float  lStepL  = (lLeft - InVoice.LastLeft) / static_cast<float>(InFrames);
        const float  lStepR  = (lRight - InVoice.LastRight) / static_cast<float>(InFrames);
        float  lGainL = InVoice.LastLeft;
        float  lGainR = InVoice.LastRight;
        double lPos   = InVoice.Cursor - lFloor;

        if (lDesc.Source.Channels == 1)
        {
            for (Uint32 lFrame = 0; lFrame < InFrames; ++lFrame, lPos += lStep)
            {
                lGainL += lStepL;
                lGainR += lStepR;

                const Uint32 lAt     = static_cast<Uint32>(lPos);
                const float  lT      = static_cast<float>(lPos - lAt);
                const float  lSample = lSource[lAt] + (lSource[lAt + 1] - lSource[lAt]) * lT;

                OutFrames[lFrame * 2]     += lSample * lGainL;
                OutFrames[lFrame * 2 + 1] += lSample * lGainR;
            }
        }
        else
        {
            for (Uint32 lFrame = 0; lFrame < InFrames; ++lFrame, lPos += lStep)
            {
                lGainL += lStepL;
                lGainR += lStepR;

                const Uint32 lAt = static_cast<Uint32>(lPos);
                const float  lT  = static_cast<float>(lPos - lAt);
                const float* lA  = lSource + lAt * 2;

                OutFrames[lFrame * 2]     += (lA[0] + (lA[2] - lA[0]) * lT) * lGainL;
                OutFrames[lFrame * 2 + 1] += (lA[1] + (lA[3] - lA[1]) * lT) * lGainR;
            }
        }

        InVoice.LastLeft  = lLeft;
        InVoice.LastRight = lRight;
        InVoice.Cursor   += InFrames * lStep;

        // Faded out after a stop.
        if (InVoice.bStopping && InVoice.Fader.IsSilent())
        {
            return true;
        }

        // Past the last frame of a sound that does not loop.
        if (!lDesc.bLoop && lValid < lCount && InVoice.Cursor >= static_cast<double>(lFirst + lValid))
        {
            return true;
        }

        // A decoded loop wraps; a stream counts on (its reader loops).
        if (lDesc.bLoop && lDesc.Source.Stream == nullptr && lDesc.Source.FrameCount > 0)
        {
            InVoice.Cursor = Maths::FMod(InVoice.Cursor, static_cast<double>(lDesc.Source.FrameCount));
        }

        return false;
    }

    Uint32 AudioMixer::FetchSource(const Voice& InVoice, const Uint64 InFirst, const Uint32 InCount)
    {
        const AudioVoiceSource& lSource   = InVoice.Desc.Source;
        float*                  lOut      = m_Source.data();
        const Uint32            lCount    = Maths::Min<Uint32>(InCount, static_cast<Uint32>(m_Source.size() / AUDIO_OUTPUT_CHANNELS));
        const size_t            lChannels = lSource.Channels;

        if (lSource.Stream != nullptr)
        {
            return lSource.Stream->Fetch(InFirst, lCount, InVoice.Desc.bLoop, lOut);
        }

        // Decoded: copy, wrapping when it loops; past the end, silence.
        Uint32 lDone = 0;
        Uint64 lAt   = InFirst;
        while (lDone < lCount)
        {
            if (lAt >= lSource.FrameCount)
            {
                if (!InVoice.Desc.bLoop)
                {
                    std::memset(lOut + lDone * lChannels, 0, sizeof(float) * (lCount - lDone) * lChannels);
                    return lDone;
                }
                lAt %= lSource.FrameCount;
            }

            const Uint32 lRun = static_cast<Uint32>(Maths::Min<Uint64>(lCount - lDone, lSource.FrameCount - lAt));
            std::memcpy(lOut + lDone * lChannels, lSource.Frames + lAt * lChannels, sizeof(float) * lRun * lChannels);
            lDone += lRun;
            lAt   += lRun;
        }

        return lCount;
    }
}
