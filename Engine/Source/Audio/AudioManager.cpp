#include "Audio/AudioManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Audio/AudioAPI.h"
#include "Audio/AudioClipResource.h"
#include "Audio/Mixer/AudioMixer.h"
#include "Audio/Mixer/AudioRingBuffer.h"
#include "Audio/Mixer/AudioSpatial.h"
#include "Audio/Mixer/AudioStreamReader.h"
#include "Audio/Mixer/SpscQueue.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/Config/EngineConfigData.h"

namespace Opaax
{
    namespace
    {
        /** Commands waiting for the audio thread at most: far more than a frame ever sends. */
        constexpr Uint32 COMMAND_CAPACITY = 4096;

        /** The audio thread keeps at least this many blocks mixed ahead of the device. */
        constexpr Uint32 MIN_BLOCKS_AHEAD = 4;

        /** The most ever mixed ahead (frames): the ring buffer's size. */
        constexpr Uint32 RING_CAPACITY_FRAMES = 16384;

        /** The audio thread wakes at least this often, should a device wake-up be missed. */
        constexpr auto MIX_THREAD_WAIT = std::chrono::milliseconds(2);

        constexpr Uint32 INVALID_SLOT = ~0u;

        /** The mixer's buses: the master, then one per EAudioBus, in its order. */
        constexpr Uint16 MASTER_BUS = 0;

        Uint16 BusIndexOf(const EAudioBus InBus) noexcept
        {
            return static_cast<Uint16>(static_cast<Uint8>(InBus) + 1);
        }

        Uint32 SecondsToFrames(const float InSeconds, const Uint32 InSampleRate) noexcept
        {
            return InSeconds > 0.f ? static_cast<Uint32>(std::lround(static_cast<double>(InSeconds) * InSampleRate)) : 0u;
        }
    }

    // =========================================================================
    // Impl — the mixer, the audio thread, and the game thread's view of the voices.
    // =========================================================================
    struct AudioManager::Impl
    {
        /** The game thread's view of one mixer voice. */
        struct Slot
        {
            Uint32 Generation = 0;
            bool   bActive    = false;   // the mixer holds it, until it reports the voice ended
            bool   bStopping  = false;   // stopped by the game: fading out, no longer playing

            Uint8             Priority    = 128;
            Uint64            StartOrder  = 0;
            float             Volume      = 1.f;
            float             Fade        = 1.f;
            Uint16            Bus         = 0;
            bool              bSpatial    = false;
            Vector2F          Position    = { 0.f, 0.f };
            float             MinDistance = 0.f;
            float             MaxDistance = 0.f;
            EAudioAttenuation Attenuation = EAudioAttenuation::Linear;

            /** What the voice reads, kept alive until it ended. */
            TSharedPtr<const AudioClipData> Data;
            TUniquePtr<AudioStreamReader>   Stream;
        };

        /** The sources of a voice replaced in the mixer, kept until it reports it ended. */
        struct Retired
        {
            SoundHandle                     Handle;
            TSharedPtr<const AudioClipData> Data;
            TUniquePtr<AudioStreamReader>   Stream;
        };

        Desc Settings;

        /** The engine's manager reads its settings from the engine config. */
        bool           bFromConfig  = false;
        Config_Engine* EngineConfig = nullptr;

        bool bStarted  = false;
        bool bHeadless = true;

        // ---- The mix ---------------------------------------------------------------------------
        TUniquePtr<TSpscQueue<AudioCommand>> Commands;   // game thread -> audio thread
        TUniquePtr<TSpscQueue<AudioEvent>>   Events;     // mixer -> game thread
        TUniquePtr<AudioMixer>               Mixer;

        // ---- The device and the audio thread (not headless) -------------------------------------
        TUniquePtr<IAudioDevice>    GivenDevice;   // used instead of the backend's
        TUniquePtr<IAudioDevice>    Device;
        TUniquePtr<AudioRingBuffer> Ring;
        Thread                      MixThread;
        TAtomic<bool>               bRunning{ false };
        TAtomic<Uint32>             FramesAhead{ 0 };
        TAtomic<Uint32>             Underruns{ 0 };
        Mutex                       WakeMutex;
        ConditionVariable           Wake;
        TDynArray<float>            ThreadBlock;

        // ---- The game thread's view --------------------------------------------------------------
        TDynArray<Slot>    Slots;
        TDynArray<Retired> RetiredSources;
        Uint64             NextStartOrder = 1;

        float    BusVolumes[AUDIO_BUS_COUNT + 1] = { 1.f, 1.f, 1.f, 1.f, 1.f };   // master first
        Vector2F Listener = { 0.f, 0.f };

        /** Headless: the fraction of a frame carried to the next Update, and where the mix goes. */
        double           PendingFrames = 0.0;
        TDynArray<float> Discard;

        bool   bWarnedCommandsFull = false;
        bool   bWarnedVoiceLimit   = false;
        Uint32 ReportedUnderruns   = 0;

        // =====================================================================
        // Commands and events
        // =====================================================================

        /** To the mixer: at once when headless, else through the audio thread's queue. */
        bool Send(const AudioCommand& InCommand)
        {
            if (bHeadless)
            {
                Mixer->Apply(InCommand);
                return true;
            }

            if (Commands->Push(InCommand))
            {
                return true;
            }

            if (!bWarnedCommandsFull)
            {
                bWarnedCommandsFull = true;
                OPAAX_LOG(LogAudio, Error, "The audio command queue is full ({} commands): commands are dropped",
                          Commands->Capacity());
            }
            return false;
        }

        bool Send(const EAudioCommand InType, const SoundHandle InHandle, const float InValue = 0.f,
                  const Uint32 InFrames = 0, const bool bInFlag = false)
        {
            AudioCommand lCommand;
            lCommand.Type   = InType;
            lCommand.Handle = InHandle;
            lCommand.Value  = InValue;
            lCommand.Frames = InFrames;
            lCommand.bFlag  = bInFlag;
            return Send(lCommand);
        }

        /** Frees what the voices that ended held. */
        void DrainEvents()
        {
            AudioEvent lEvent;
            while (Events->Pop(lEvent))
            {
                const Uint32 lIndex = lEvent.Handle.GetIndex();
                if (lIndex < Slots.size() && Slots[lIndex].bActive && Slots[lIndex].Generation == lEvent.Handle.GetGeneration())
                {
                    Release(Slots[lIndex]);
                    continue;
                }

                // A voice that was replaced: its sources were set apart.
                RetiredSources.erase(std::remove_if(RetiredSources.begin(), RetiredSources.end(),
                                                    [&lEvent](const Retired& InRetired) { return InRetired.Handle == lEvent.Handle; }),
                                     RetiredSources.end());
            }
        }

        static void Release(Slot& InSlot)
        {
            InSlot.bActive   = false;
            InSlot.bStopping = false;
            InSlot.Data.reset();
            InSlot.Stream.reset();
        }

        /** Headless: InFrames more of the mix, heard by no one. */
        void MixHeadless(Uint64 InFrames)
        {
            const Uint32 lBlock = Mixer->GetBlockFrames();
            Discard.resize(static_cast<size_t>(lBlock) * AUDIO_OUTPUT_CHANNELS);

            while (InFrames > 0)
            {
                const Uint32 lFrames = static_cast<Uint32>(std::min<Uint64>(InFrames, lBlock));
                Mixer->Mix(Discard.data(), lFrames);
                InFrames -= lFrames;
            }
        }

        // =====================================================================
        // The audio thread and the device
        // =====================================================================

        /** Keeps the ring buffer FramesAhead full, applying commands between blocks. */
        void ThreadLoop()
        {
            const Uint32 lBlock = Mixer->GetBlockFrames();

            while (bRunning.load(std::memory_order_acquire))
            {
                AudioCommand lCommand;
                while (Commands->Pop(lCommand))
                {
                    Mixer->Apply(lCommand);
                }

                while (Ring->GetReadableFrames() < FramesAhead.load(std::memory_order_relaxed) && Ring->GetFreeFrames() >= lBlock)
                {
                    Mixer->Mix(ThreadBlock.data(), lBlock);
                    Ring->Write(ThreadBlock.data(), lBlock);

                    while (Commands->Pop(lCommand))
                    {
                        Mixer->Apply(lCommand);
                    }
                }

                // Paced by the device: it wakes this thread each time it takes frames.
                std::unique_lock<Mutex> lLock(WakeMutex);
                Wake.wait_for(lLock, MIX_THREAD_WAIT, [this]()
                {
                    return !bRunning.load(std::memory_order_acquire)
                        || Ring->GetReadableFrames() < FramesAhead.load(std::memory_order_relaxed);
                });
            }
        }

        /** The device's thread: copies what is mixed, never waits. */
        static void OnDeviceData(void* InUser, float* OutFrames, const Uint32 InFrameCount)
        {
            Impl& lSelf = *static_cast<Impl*>(InUser);

            const Uint32 lRead = lSelf.Ring->Read(OutFrames, InFrameCount);
            if (lRead < InFrameCount)
            {
                std::memset(OutFrames + static_cast<size_t>(lRead) * AUDIO_OUTPUT_CHANNELS, 0,
                            sizeof(float) * (InFrameCount - lRead) * AUDIO_OUTPUT_CHANNELS);
                lSelf.Underruns.fetch_add(1, std::memory_order_relaxed);
            }

            lSelf.Wake.notify_one();
        }

        /** Starts the audio thread, then the device. False when there is no device (nothing left running). */
        bool StartDevice()
        {
            const Uint32 lBlock = Mixer->GetBlockFrames();

            Device = GivenDevice != nullptr ? Move(GivenDevice) : AudioAPI::CreateDevice(Settings.Backend);
            if (Device == nullptr)
            {
                return false;
            }

            Commands = MakeUnique<TSpscQueue<AudioCommand>>(COMMAND_CAPACITY);
            Ring     = MakeUnique<AudioRingBuffer>(RING_CAPACITY_FRAMES, AUDIO_OUTPUT_CHANNELS);
            ThreadBlock.resize(static_cast<size_t>(lBlock) * AUDIO_OUTPUT_CHANNELS);
            FramesAhead.store(lBlock * MIN_BLOCKS_AHEAD);

            // The thread first, so the device finds frames ready.
            bHeadless = false;
            bRunning.store(true, std::memory_order_release);
            MixThread = Thread([this]() { ThreadLoop(); });

            AudioDeviceDesc lDesc;
            lDesc.SampleRate   = Settings.SampleRate;
            lDesc.Channels     = AUDIO_OUTPUT_CHANNELS;
            lDesc.PeriodFrames = lBlock;

            if (!Device->Start(lDesc, &Impl::OnDeviceData, this))
            {
                StopThread();
                Device.reset();
                Commands.reset();
                Ring.reset();
                bHeadless = true;
                return false;
            }

            // Ahead by the device's period and two blocks more, so a late wake-up is not a gap.
            const Uint32 lPeriod = Device->GetPeriodFrames();
            const Uint32 lAhead  = std::max(lBlock * MIN_BLOCKS_AHEAD, ((lPeriod + 2 * lBlock + lBlock - 1) / lBlock) * lBlock);
            FramesAhead.store(std::min(lAhead, RING_CAPACITY_FRAMES - lBlock));
            Wake.notify_one();
            return true;
        }

        void StopThread()
        {
            if (!MixThread.joinable())
            {
                return;
            }

            bRunning.store(false, std::memory_order_release);
            Wake.notify_all();
            MixThread.join();
        }

        // =====================================================================
        // Voices
        // =====================================================================
        Slot* Find(const SoundHandle InSound)
        {
            const Uint32 lIndex = InSound.GetIndex();
            if (!InSound.IsValid() || lIndex >= Slots.size())
            {
                return nullptr;
            }

            Slot& lSlot = Slots[lIndex];
            return (lSlot.bActive && lSlot.Generation == InSound.GetGeneration()) ? &lSlot : nullptr;
        }

        /** A playing sound (not stopping) for InSound, or null. */
        Slot* FindPlaying(const SoundHandle InSound)
        {
            Slot* lSlot = Find(InSound);
            return (lSlot != nullptr && !lSlot->bStopping) ? lSlot : nullptr;
        }

        /** About how loud a voice is now: its volume, its buses and its distance. */
        float EstimatedGain(const Slot& InSlot) const
        {
            float lGain = InSlot.Volume * InSlot.Fade * BusVolumes[MASTER_BUS]
                        * (InSlot.Bus != MASTER_BUS && InSlot.Bus <= AUDIO_BUS_COUNT ? BusVolumes[InSlot.Bus] : 1.f);

            if (InSlot.bSpatial)
            {
                const Vector2F lOffset = InSlot.Position - Listener;
                lGain *= AudioSpatial::Attenuate(InSlot.Attenuation, std::sqrt(lOffset.x * lOffset.x + lOffset.y * lOffset.y),
                                                 InSlot.MinDistance, InSlot.MaxDistance);
            }
            return lGain;
        }

        /**
         * A voice for a new sound of InPriority: a free one, else the one least worth keeping (already
         * stopping, then the lowest priority, the quietest, the oldest).
         * @return INVALID_SLOT when every voice plays something of a higher priority
         */
        Uint32 PickSlot(const Uint8 InPriority)
        {
            for (Uint32 lIndex = 0; lIndex < Slots.size(); ++lIndex)
            {
                if (!Slots[lIndex].bActive)
                {
                    return lIndex;
                }
            }

            Uint32 lVictim = 0;
            for (Uint32 lIndex = 1; lIndex < Slots.size(); ++lIndex)
            {
                const Slot& lA = Slots[lIndex];
                const Slot& lB = Slots[lVictim];
                if (lA.bStopping != lB.bStopping)  { if (lA.bStopping) { lVictim = lIndex; } continue; }
                if (lA.Priority != lB.Priority)    { if (lA.Priority < lB.Priority) { lVictim = lIndex; } continue; }

                const float lGainA = EstimatedGain(lA);
                const float lGainB = EstimatedGain(lB);
                if (lGainA != lGainB)              { if (lGainA < lGainB) { lVictim = lIndex; } continue; }
                if (lA.StartOrder < lB.StartOrder) { lVictim = lIndex; }
            }

            const Slot& lChosen = Slots[lVictim];
            if (!lChosen.bStopping && lChosen.Priority > InPriority)
            {
                return INVALID_SLOT;
            }

            if (!bWarnedVoiceLimit)
            {
                bWarnedVoiceLimit = true;
                OPAAX_LOG(LogAudio, Warn, "{} sounds are already playing (Audio.MaxVoices): new ones replace the least "
                          "important", Slots.size());
            }
            return lVictim;
        }

        /** Sets a replaced voice's sources apart until the mixer is done with them. */
        void Retire(Slot& InSlot, const Uint32 InIndex)
        {
            RetiredSources.push_back(Retired{ SoundHandle::Make(InIndex, InSlot.Generation), Move(InSlot.Data), Move(InSlot.Stream) });
            Release(InSlot);
        }
    };

    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    AudioManager::AudioManager()
        : m_Impl(MakeUnique<Impl>())
    {
        m_Impl->bFromConfig = true;
    }

    AudioManager::AudioManager(const Desc& InDesc)
        : m_Impl(MakeUnique<Impl>())
    {
        m_Impl->Settings = InDesc;
    }

    AudioManager::AudioManager(const Desc& InDesc, TUniquePtr<IAudioDevice> InDevice)
        : m_Impl(MakeUnique<Impl>())
    {
        m_Impl->Settings    = InDesc;
        m_Impl->GivenDevice = Move(InDevice);
    }

    AudioManager::~AudioManager()
    {
        Shutdown();
    }

    // =========================================================================
    // ISubsystem
    // =========================================================================
    bool AudioManager::Startup()
    {
        Impl& lImpl = *m_Impl;
        if (lImpl.bStarted)
        {
            return true;
        }

        const AudioSettings* lSettings = nullptr;
        if (lImpl.bFromConfig)
        {
            lImpl.EngineConfig = &OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>();
            lSettings = &lImpl.EngineConfig->GetData().Audio;

            lImpl.Settings.bHeadless = !lSettings->bEnabled;
            lImpl.Settings.Backend   = lSettings->Backend;
            lImpl.Settings.MaxVoices = lSettings->MaxVoices;
        }

        lImpl.Settings.SampleRate  = std::clamp(lImpl.Settings.SampleRate, 8000u, 192000u);
        lImpl.Settings.MaxVoices   = std::max(lImpl.Settings.MaxVoices, 1u);
        lImpl.Settings.BlockFrames = std::clamp(lImpl.Settings.BlockFrames, 32u, 4096u);
        AudioAPI::SetBackend(lImpl.Settings.Backend);

        // The bus tree: the master, then one bus per EAudioBus under it.
        AudioMixer::Desc lMix;
        lMix.SampleRate  = lImpl.Settings.SampleRate;
        lMix.MaxVoices   = lImpl.Settings.MaxVoices;
        lMix.BlockFrames = lImpl.Settings.BlockFrames;
        lMix.Buses.push_back(AudioBusDesc{ OPAAX_ID("Master"), -1, lImpl.BusVolumes[MASTER_BUS] });
        for (Uint8 lBus = 0; lBus < AUDIO_BUS_COUNT; ++lBus)
        {
            lMix.Buses.push_back(AudioBusDesc{ OPAAX_ID(ToString(static_cast<EAudioBus>(lBus))), MASTER_BUS, lImpl.BusVolumes[lBus + 1] });
        }

        // Every voice and every tail can end before the game thread drains: room for all of them twice.
        lImpl.Events = MakeUnique<TSpscQueue<AudioEvent>>(2 * (lImpl.Settings.MaxVoices + AudioMixer::TAIL_VOICES) + 16);
        lImpl.Mixer  = MakeUnique<AudioMixer>(lMix, *lImpl.Events);
        lImpl.Slots.clear();
        lImpl.Slots.resize(lImpl.Settings.MaxVoices);

        bool bDevice = false;
        if (!lImpl.Settings.bHeadless)
        {
            bDevice = lImpl.StartDevice();
            if (!bDevice)
            {
                OPAAX_LOG(LogAudio, Warn, "No audio device could be opened: sounds will play silently");
            }
        }

        lImpl.bHeadless = !bDevice;
        lImpl.bStarted  = true;

        if (lSettings != nullptr)
        {
            ApplySettings(*lSettings);

            // Volumes follow the settings while the game runs.
            lImpl.EngineConfig->OnChanged().AddMember(this, &AudioManager::OnEngineConfigChanged);
        }

        OPAAX_LOG(LogAudio, Info, "Audio started — {}, {} Hz, {} voices{}", ToString(lImpl.Settings.Backend),
                  lImpl.Settings.SampleRate, lImpl.Settings.MaxVoices,
                  lImpl.bHeadless ? ", headless (not heard)" : fmt::format(", on '{}'", lImpl.Device->GetName()));
        return true;
    }

    void AudioManager::Update(const double InDeltaTime)
    {
        Impl& lImpl = *m_Impl;
        if (!lImpl.bStarted)
        {
            return;
        }

        // Nothing pulls a headless mix: it advances by the frame's time, so sounds end on time.
        if (lImpl.bHeadless && InDeltaTime > 0.0)
        {
            lImpl.PendingFrames += InDeltaTime * static_cast<double>(lImpl.Settings.SampleRate);
            const Uint64 lFrames = static_cast<Uint64>(lImpl.PendingFrames);
            lImpl.PendingFrames -= static_cast<double>(lFrames);

            lImpl.MixHeadless(lFrames);
        }

        lImpl.DrainEvents();

        const Uint32 lUnderruns = lImpl.Underruns.load(std::memory_order_relaxed);
        if (lImpl.ReportedUnderruns == 0 && lUnderruns > 0)
        {
            OPAAX_LOG(LogAudio, Warn, "The audio device ran out of mixed sound (a gap was heard); later gaps are "
                      "counted in GetUnderrunCount");
        }
        lImpl.ReportedUnderruns = lUnderruns;
    }

    void AudioManager::Shutdown()
    {
        if (!m_Impl || !m_Impl->bStarted)
        {
            return;
        }

        Impl& lImpl = *m_Impl;
        if (lImpl.EngineConfig != nullptr)
        {
            lImpl.EngineConfig->OnChanged().RemoveAll(this);
            lImpl.EngineConfig = nullptr;
        }

        // The device first: no callback reads the ring once it is stopped. Then the thread.
        if (lImpl.Device != nullptr)
        {
            lImpl.Device->Stop();
        }
        lImpl.StopThread();

        lImpl.Device.reset();
        lImpl.Mixer.reset();
        lImpl.Ring.reset();
        lImpl.Commands.reset();
        lImpl.Events.reset();
        lImpl.Slots.clear();
        lImpl.RetiredSources.clear();

        lImpl.bStarted  = false;
        lImpl.bHeadless = true;

        OPAAX_LOG(LogAudio, Trace, "Audio stopped");
    }

    // =========================================================================
    // Sounds
    // =========================================================================
    SoundHandle AudioManager::Play(const AudioClipResource& InClip, const PlaySoundParams& InParams)
    {
        Impl& lImpl = *m_Impl;
        if (!lImpl.bStarted || InClip.IsSilent())
        {
            return SoundHandle{};
        }

        const AudioClipData& lData = *InClip.Data;

        AudioVoiceDesc lVoice;
        lVoice.Source.Channels   = lData.Format.Channels;
        lVoice.Source.SampleRate = lData.Format.SampleRate;

        // A streamed clip gets its own decoder, made here: the audio thread never allocates.
        TUniquePtr<AudioStreamReader> lStream;
        if (lData.IsStreamed())
        {
            TUniquePtr<IAudioDecoder> lDecoder = AudioAPI::OpenDecoder(lData.Encoded.data(), lData.Encoded.size());
            if (lDecoder == nullptr)
            {
                OPAAX_LOG(LogAudio, Error, "A sound could not start: its clip does not decode");
                return SoundHandle{};
            }

            lStream = MakeUnique<AudioStreamReader>(Move(lDecoder), AudioMixer::GetStreamWindowFrames(lImpl.Mixer->GetBlockFrames()));
            lVoice.Source.Stream = lStream.get();
        }
        else
        {
            lVoice.Source.Frames     = lData.Frames.data();
            lVoice.Source.FrameCount = lData.Format.FrameCount;
        }

        const Uint32 lIndex = lImpl.PickSlot(InParams.Priority);
        if (lIndex == INVALID_SLOT)
        {
            return SoundHandle{};
        }

        Impl::Slot& lSlot = lImpl.Slots[lIndex];
        if (lSlot.bActive)
        {
            lImpl.Retire(lSlot, lIndex);
        }

        // Generation 0 is never used: it is the invalid handle.
        lSlot.Generation = lSlot.Generation == ~0u ? 1u : lSlot.Generation + 1;
        const SoundHandle lHandle = SoundHandle::Make(lIndex, lSlot.Generation);

        lVoice.Volume       = std::max(InParams.Volume, 0.f);
        lVoice.Pitch        = InParams.Pitch;
        lVoice.Bus          = BusIndexOf(InParams.Bus);
        lVoice.bLoop        = InParams.bLoop;
        lVoice.FadeInFrames = SecondsToFrames(InParams.FadeInSeconds, lImpl.Settings.SampleRate);
        lVoice.bSpatial     = InParams.bSpatial;
        lVoice.Position     = InParams.Position;
        lVoice.MinDistance  = InParams.MinDistance;
        lVoice.MaxDistance  = InParams.MaxDistance;

        AudioCommand lCommand;
        lCommand.Type   = EAudioCommand::Play;
        lCommand.Handle = lHandle;
        lCommand.Voice  = lVoice;
        if (!lImpl.Send(lCommand))
        {
            return SoundHandle{};
        }

        lSlot.bActive     = true;
        lSlot.bStopping   = false;
        lSlot.Priority    = InParams.Priority;
        lSlot.StartOrder  = lImpl.NextStartOrder++;
        lSlot.Volume      = lVoice.Volume;
        lSlot.Fade        = 1.f;
        lSlot.Bus         = lVoice.Bus;
        lSlot.bSpatial    = lVoice.bSpatial;
        lSlot.Position    = lVoice.Position;
        lSlot.MinDistance = lVoice.MinDistance;
        lSlot.MaxDistance = lVoice.MaxDistance;
        lSlot.Attenuation = lVoice.Attenuation;
        lSlot.Data        = InClip.Data;
        lSlot.Stream      = Move(lStream);
        return lHandle;
    }

    void AudioManager::Stop(const SoundHandle InSound, const float InFadeOutSeconds)
    {
        if (Impl::Slot* lSlot = m_Impl->FindPlaying(InSound))
        {
            lSlot->bStopping = true;
            m_Impl->Send(EAudioCommand::Stop, InSound, 0.f, SecondsToFrames(InFadeOutSeconds, m_Impl->Settings.SampleRate));
        }
    }

    void AudioManager::StopAll(const float InFadeOutSeconds)
    {
        Impl& lImpl = *m_Impl;
        if (!lImpl.bStarted)
        {
            return;
        }

        for (Impl::Slot& lSlot : lImpl.Slots)
        {
            lSlot.bStopping = lSlot.bActive;
        }

        AudioCommand lCommand;
        lCommand.Type   = EAudioCommand::StopAll;
        lCommand.Frames = SecondsToFrames(InFadeOutSeconds, lImpl.Settings.SampleRate);
        lImpl.Send(lCommand);
    }

    bool AudioManager::IsPlaying(const SoundHandle InSound) const
    {
        return m_Impl->FindPlaying(InSound) != nullptr;
    }

    void AudioManager::SetVolume(const SoundHandle InSound, const float InVolume)
    {
        if (Impl::Slot* lSlot = m_Impl->Find(InSound))
        {
            lSlot->Volume = std::max(InVolume, 0.f);
            m_Impl->Send(EAudioCommand::SetVolume, InSound, lSlot->Volume);
        }
    }

    void AudioManager::FadeTo(const SoundHandle InSound, const float InVolume, const float InSeconds)
    {
        if (Impl::Slot* lSlot = m_Impl->FindPlaying(InSound))
        {
            lSlot->Fade = std::max(InVolume, 0.f);
            m_Impl->Send(EAudioCommand::FadeVolume, InSound, lSlot->Fade, SecondsToFrames(InSeconds, m_Impl->Settings.SampleRate));
        }
    }

    void AudioManager::SetPitch(const SoundHandle InSound, const float InPitch)
    {
        if (m_Impl->Find(InSound) != nullptr)
        {
            m_Impl->Send(EAudioCommand::SetPitch, InSound, InPitch);
        }
    }

    void AudioManager::SetPosition(const SoundHandle InSound, const Vector2F InPosition)
    {
        if (Impl::Slot* lSlot = m_Impl->Find(InSound))
        {
            lSlot->Position = InPosition;

            AudioCommand lCommand;
            lCommand.Type     = EAudioCommand::SetPosition;
            lCommand.Handle   = InSound;
            lCommand.Position = InPosition;
            m_Impl->Send(lCommand);
        }
    }

    void AudioManager::SetPaused(const SoundHandle InSound, const bool bInPaused)
    {
        if (m_Impl->Find(InSound) != nullptr)
        {
            m_Impl->Send(EAudioCommand::SetPaused, InSound, 0.f, 0, bInPaused);
        }
    }

    // =========================================================================
    // Mix
    // =========================================================================
    void AudioManager::SetBusVolume(const EAudioBus InBus, const float InVolume)
    {
        const Uint16 lBus = BusIndexOf(InBus);
        if (lBus > AUDIO_BUS_COUNT)
        {
            return;
        }

        m_Impl->BusVolumes[lBus] = std::max(InVolume, 0.f);
        if (m_Impl->bStarted)
        {
            AudioCommand lCommand;
            lCommand.Type  = EAudioCommand::SetBusVolume;
            lCommand.Bus   = lBus;
            lCommand.Value = m_Impl->BusVolumes[lBus];
            m_Impl->Send(lCommand);
        }
    }

    float AudioManager::GetBusVolume(const EAudioBus InBus) const
    {
        const Uint16 lBus = BusIndexOf(InBus);
        return lBus <= AUDIO_BUS_COUNT ? m_Impl->BusVolumes[lBus] : 0.f;
    }

    void AudioManager::SetMasterVolume(const float InVolume)
    {
        m_Impl->BusVolumes[MASTER_BUS] = std::max(InVolume, 0.f);
        if (m_Impl->bStarted)
        {
            AudioCommand lCommand;
            lCommand.Type  = EAudioCommand::SetBusVolume;
            lCommand.Bus   = MASTER_BUS;
            lCommand.Value = m_Impl->BusVolumes[MASTER_BUS];
            m_Impl->Send(lCommand);
        }
    }

    float AudioManager::GetMasterVolume() const
    {
        return m_Impl->BusVolumes[MASTER_BUS];
    }

    void AudioManager::SetListenerPosition(const Vector2F InPosition)
    {
        m_Impl->Listener = InPosition;
        if (m_Impl->bStarted)
        {
            AudioCommand lCommand;
            lCommand.Type     = EAudioCommand::SetListener;
            lCommand.Position = InPosition;
            m_Impl->Send(lCommand);
        }
    }

    Vector2F AudioManager::GetListenerPosition() const
    {
        return m_Impl->Listener;
    }

    void AudioManager::ApplySettings(const AudioSettings& InSettings)
    {
        SetMasterVolume(InSettings.MasterVolume);
        SetBusVolume(EAudioBus::Music,    InSettings.MusicVolume);
        SetBusVolume(EAudioBus::Effects,  InSettings.EffectsVolume);
        SetBusVolume(EAudioBus::Ambience, InSettings.AmbienceVolume);
        SetBusVolume(EAudioBus::UI,       InSettings.UIVolume);
    }

    void AudioManager::OnEngineConfigChanged()
    {
        if (m_Impl->EngineConfig != nullptr)
        {
            ApplySettings(m_Impl->EngineConfig->GetData().Audio);
        }
    }

    // =========================================================================
    // State
    // =========================================================================
    bool AudioManager::IsStarted() const noexcept
    {
        return m_Impl->bStarted;
    }

    bool AudioManager::IsHeadless() const noexcept
    {
        return m_Impl->bHeadless;
    }

    Uint64 AudioManager::GetVoiceCount() const noexcept
    {
        return static_cast<Uint64>(std::count_if(m_Impl->Slots.begin(), m_Impl->Slots.end(),
                                                 [](const Impl::Slot& InSlot) { return InSlot.bActive && !InSlot.bStopping; }));
    }

    Uint64 AudioManager::RenderFrames(float* OutFrames, const Uint64 InFrameCount)
    {
        Impl& lImpl = *m_Impl;
        if (!lImpl.bStarted || !lImpl.bHeadless || OutFrames == nullptr)
        {
            return 0;
        }

        Uint64 lDone = 0;
        while (lDone < InFrameCount)
        {
            const Uint32 lFrames = static_cast<Uint32>(std::min<Uint64>(InFrameCount - lDone, 1u << 20));
            lImpl.Mixer->Mix(OutFrames + lDone * AUDIO_OUTPUT_CHANNELS, lFrames);
            lDone += lFrames;
        }

        lImpl.DrainEvents();
        return InFrameCount;
    }

    Uint32 AudioManager::GetSampleRate() const noexcept
    {
        return m_Impl->bStarted ? m_Impl->Settings.SampleRate : 0;
    }

    Uint32 AudioManager::GetChannels() const noexcept
    {
        return m_Impl->bStarted ? AUDIO_OUTPUT_CHANNELS : 0;
    }

    Uint32 AudioManager::GetUnderrunCount() const noexcept
    {
        return m_Impl->Underruns.load(std::memory_order_relaxed);
    }
}
