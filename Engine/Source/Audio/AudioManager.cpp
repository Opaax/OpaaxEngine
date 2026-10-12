#include "Audio/AudioManager.h"

#include "Core/Algo/Algo.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Maths.h"
#include <cstring>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Audio/AudioAPI.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioRandom.h"
#include "Audio/Mixer/AudioMixer.h"
#include "Audio/Mixer/AudioRingBuffer.h"
#include "Audio/Mixer/AudioSpatial.h"
#include "Audio/Mixer/AudioStreamReader.h"
#include "Audio/Mixer/SpscQueue.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/Config/EngineConfigData.h"
#include "Resources/ResourceManager.h"

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
        constexpr auto MIX_THREAD_WAIT = Milliseconds(2);

        constexpr Uint32 INVALID_SLOT = ~0u;

        /** The master is the mixer's first bus. */
        constexpr Uint16 MASTER_BUS = 0;

        Uint32 SecondsToFrames(const float InSeconds, const Uint32 InSampleRate) noexcept
        {
            return InSeconds > 0.f ? static_cast<Uint32>(Maths::RoundHalfFromZero(static_cast<double>(InSeconds) * InSampleRate)) : 0u;
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
            bool   bPending   = false;   // reserved for a clip still loading: the mixer has not seen it

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

        /** A sound whose clip is still loading, and what was asked of it meanwhile. */
        struct PendingPlay
        {
            SoundHandle                    Handle;
            ResourceRef<AudioClipResource> Clip;
            PlaySoundParams                Params;
            bool                           bPaused    = false;
            float                          FadeTarget = -1.f;   // -1: no FadeTo asked
            Uint32                         FadeFrames = 0;
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
        TDynArray<Slot>        Slots;
        TDynArray<Retired>     RetiredSources;
        TDynArray<PendingPlay> Pending;
        Uint64                 NextStartOrder = 1;

        AudioRandom                                       Random;
        TUnorderedMap<OpaaxStringID, AudioSelectionState> Selections;   // by clip definition

        /** The game thread's view of a bus: what the mixer was last told. */
        struct BusView
        {
            OpaaxStringID Id;
            Int32         Parent = -1;
            float         Volume = 1.f;
            float         Fade   = 1.f;
            bool          bMuted = false;
        };

        TDynArray<BusView>                   Buses;        // the mixer's order: the master first
        TUnorderedMap<OpaaxStringID, Uint16> BusIndices;
        TUnorderedSet<OpaaxStringID>         WarnedBuses;  // names not found, already logged

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
                Algo::RemoveIf(RetiredSources, [&lEvent](const Retired& InRetired) { return InRetired.Handle == lEvent.Handle; });
            }
        }

        static void Release(Slot& InSlot)
        {
            InSlot.bActive   = false;
            InSlot.bStopping = false;
            InSlot.bPending  = false;
            InSlot.Data.reset();
            InSlot.Stream.reset();
        }

        PendingPlay* FindPending(const SoundHandle InSound)
        {
            for (PendingPlay& lPending : Pending)
            {
                if (lPending.Handle == InSound) { return &lPending; }
            }
            return nullptr;
        }

        /** Forgets a sound still waiting for its clip: the mixer never had it. */
        void CancelPending(const SoundHandle InSound)
        {
            Algo::RemoveIf(Pending, [InSound](const PendingPlay& InPending) { return InPending.Handle == InSound; });

            if (Slot* lSlot = Find(InSound))
            {
                Release(*lSlot);
            }
        }

        /** Starts the sounds whose clip finished loading, and frees those whose clip failed. */
        void UpdatePending()
        {
            for (size_t lIndex = 0; lIndex < Pending.size();)
            {
                PendingPlay&         lPending = Pending[lIndex];
                const EResourceState lState   = lPending.Clip.GetManager()->GetState<AudioClipResource>(lPending.Clip.GetHandle());
                if (lState == EResourceState::Loading)
                {
                    ++lIndex;
                    continue;
                }

                // Moved out first: starting may log, and nothing else touches the list meanwhile.
                PendingPlay lReady = Move(lPending);
                Pending.erase(Pending.begin() + static_cast<PtrDiff>(lIndex));

                Slot* lSlot = Find(lReady.Handle);
                if (lSlot == nullptr)
                {
                    continue;
                }

                const AudioClipResource* lClip = lReady.Clip.Get();
                if (lState != EResourceState::Loaded || lClip == nullptr || lClip->IsSilent()
                    || !StartVoice(lReady.Handle, *lClip, lReady.Params))
                {
                    Release(*lSlot);
                    continue;
                }

                // What was asked while it loaded.
                if (lReady.bPaused)
                {
                    Send(EAudioCommand::SetPaused, lReady.Handle, 0.f, 0, true);
                }
                if (lReady.FadeTarget >= 0.f)
                {
                    Send(EAudioCommand::FadeVolume, lReady.Handle, lReady.FadeTarget, lReady.FadeFrames);
                }
            }
        }

        /** Headless: InFrames more of the mix, heard by no one. */
        void MixHeadless(Uint64 InFrames)
        {
            const Uint32 lBlock = Mixer->GetBlockFrames();
            Discard.resize(static_cast<size_t>(lBlock) * AUDIO_OUTPUT_CHANNELS);

            while (InFrames > 0)
            {
                const Uint32 lFrames = static_cast<Uint32>(Maths::Min<Uint64>(InFrames, lBlock));
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

            while (bRunning.load(EMemoryOrder::Acquire))
            {
                AudioCommand lCommand;
                while (Commands->Pop(lCommand))
                {
                    Mixer->Apply(lCommand);
                }

                while (Ring->GetReadableFrames() < FramesAhead.load(EMemoryOrder::Relaxed) && Ring->GetFreeFrames() >= lBlock)
                {
                    Mixer->Mix(ThreadBlock.data(), lBlock);
                    Ring->Write(ThreadBlock.data(), lBlock);

                    while (Commands->Pop(lCommand))
                    {
                        Mixer->Apply(lCommand);
                    }
                }

                // Paced by the device: it wakes this thread each time it takes frames.
                TUniqueLock<Mutex> lLock(WakeMutex);
                Wake.wait_for(lLock, MIX_THREAD_WAIT, [this]()
                {
                    return !bRunning.load(EMemoryOrder::Acquire)
                        || Ring->GetReadableFrames() < FramesAhead.load(EMemoryOrder::Relaxed);
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
                lSelf.Underruns.fetch_add(1, EMemoryOrder::Relaxed);
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
            bRunning.store(true, EMemoryOrder::Release);
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
            const Uint32 lAhead  = Maths::Max(lBlock * MIN_BLOCKS_AHEAD, ((lPeriod + 2 * lBlock + lBlock - 1) / lBlock) * lBlock);
            FramesAhead.store(Maths::Min(lAhead, RING_CAPACITY_FRAMES - lBlock));
            Wake.notify_one();
            return true;
        }

        void StopThread()
        {
            if (!MixThread.joinable())
            {
                return;
            }

            bRunning.store(false, EMemoryOrder::Release);
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

        // =====================================================================
        // Buses
        // =====================================================================

        /**
         * The bus tree from InBuses, under the master: a parent is placed before its children. A bus
         * without a name, listed twice, or under a parent that is not a bus (or under itself) is
         * logged; the last two go under the master.
         */
        void BuildBuses(const TDynArray<AudioBusSettings>& InBuses)
        {
            Buses.clear();
            BusIndices.clear();
            Buses.push_back(BusView{ AudioMasterBus(), -1 });
            BusIndices.emplace(AudioMasterBus(), MASTER_BUS);

            TDynArray<const AudioBusSettings*> lPending;
            TUnorderedSet<OpaaxStringID>       lNames;
            for (const AudioBusSettings& lBus : InBuses)
            {
                if (!lBus.Name.IsValid())
                {
                    OPAAX_LOG(LogAudio, Warn, "An audio bus has no name (Audio.Buses): it is left out");
                    continue;
                }

                if (lBus.Name == AudioMasterBus() || !lNames.insert(lBus.Name).second)
                {
                    OPAAX_LOG(LogAudio, Warn, "Audio bus '{}' is listed twice (Audio.Buses; the master is always there): "
                              "the first one is kept", lBus.Name);
                    continue;
                }
                lPending.push_back(&lBus);
            }

            // Place every bus whose parent is placed, until none moves.
            bool bPlaced = true;
            while (!lPending.empty() && bPlaced)
            {
                bPlaced = false;
                for (auto lIt = lPending.begin(); lIt != lPending.end();)
                {
                    const OpaaxStringID lParent = (*lIt)->Parent.IsValid() ? (*lIt)->Parent : AudioMasterBus();
                    const auto          lFound  = BusIndices.find(lParent);
                    if (lFound == BusIndices.end())
                    {
                        ++lIt;
                        continue;
                    }

                    AddBus(**lIt, lFound->second);
                    lIt     = lPending.erase(lIt);
                    bPlaced = true;
                }
            }

            for (const AudioBusSettings* lBus : lPending)
            {
                OPAAX_LOG(LogAudio, Warn, "Audio bus '{}' is under '{}', which is not a bus above it (Audio.Buses): it is "
                          "put under the master", lBus->Name, lBus->Parent);
                AddBus(*lBus, MASTER_BUS);
            }
        }

        void AddBus(const AudioBusSettings& InBus, const Uint16 InParent)
        {
            BusIndices.emplace(InBus.Name, static_cast<Uint16>(Buses.size()));
            Buses.push_back(BusView{ InBus.Name, InParent, Maths::Max(InBus.Volume, 0.f) });
        }

        /** A bus by name, or -1. Unknown names are logged once. */
        Int32 FindBus(const OpaaxStringID InBus)
        {
            const auto lFound = BusIndices.find(InBus);
            if (lFound != BusIndices.end())
            {
                return lFound->second;
            }

            if (bStarted && WarnedBuses.insert(InBus).second)
            {
                OPAAX_LOG(LogAudio, Warn, "No audio bus is named '{}' (Engine.config, Audio.Buses)", InBus);
            }
            return -1;
        }

        /** The bus a sound plays in: its own, the default one when it names none, else the master. */
        Uint16 ResolveBus(const OpaaxStringID InBus)
        {
            // Maps saved before the bus tree name the effects bus "Effects".
            static const OpaaxStringID s_OldEffects = OPAAX_ID("Effects");

            OpaaxStringID lName = InBus.IsValid() ? InBus : AudioDefaultBus();
            if (lName == s_OldEffects && !BusIndices.contains(lName))
            {
                lName = AudioDefaultBus();
            }

            const Int32 lBus = FindBus(lName);
            return lBus >= 0 ? static_cast<Uint16>(lBus) : MASTER_BUS;
        }

        /** A bus's gain through its chain up to the master, as last set (fades at their target). */
        float ChainGain(Int32 InBus) const
        {
            float lGain = 1.f;
            while (InBus >= 0 && InBus < static_cast<Int32>(Buses.size()))
            {
                const BusView& lBus = Buses[InBus];
                lGain *= lBus.bMuted ? 0.f : lBus.Volume * lBus.Fade;
                InBus  = lBus.Parent;
            }
            return lGain;
        }

        void SendBus(const EAudioCommand InType, const Uint16 InBus, const float InValue, const Uint32 InFrames = 0,
                     const bool bInFlag = false)
        {
            AudioCommand lCommand;
            lCommand.Type   = InType;
            lCommand.Bus    = InBus;
            lCommand.Value  = InValue;
            lCommand.Frames = InFrames;
            lCommand.bFlag  = bInFlag;
            Send(lCommand);
        }

        /** About how loud a voice is now: its volume, its buses and its distance. */
        float EstimatedGain(const Slot& InSlot) const
        {
            float lGain = InSlot.Volume * InSlot.Fade * ChainGain(InSlot.Bus);

            if (InSlot.bSpatial)
            {
                const Vector2F lOffset = InSlot.Position - Listener;
                lGain *= AudioSpatial::Attenuate(InSlot.Attenuation, Maths::Sqrt(lOffset.x * lOffset.x + lOffset.y * lOffset.y),
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
            const SoundHandle lHandle = SoundHandle::Make(InIndex, InSlot.Generation);
            if (InSlot.bPending)
            {
                CancelPending(lHandle);
                return;
            }

            RetiredSources.push_back(Retired{ lHandle, Move(InSlot.Data), Move(InSlot.Stream) });
            Release(InSlot);
        }

        /**
         * A voice for a new sound, its view filled from InParams (the voice still to start).
         * @return Invalid when every voice plays something of a higher priority
         */
        SoundHandle Reserve(const PlaySoundParams& InParams)
        {
            const Uint32 lIndex = PickSlot(InParams.Priority);
            if (lIndex == INVALID_SLOT)
            {
                return SoundHandle{};
            }

            Slot& lSlot = Slots[lIndex];
            if (lSlot.bActive)
            {
                Retire(lSlot, lIndex);
            }

            // Generation 0 is never used: it is the invalid handle.
            lSlot.Generation  = lSlot.Generation == ~0u ? 1u : lSlot.Generation + 1;
            lSlot.bActive     = true;
            lSlot.bStopping   = false;
            lSlot.bPending    = false;
            lSlot.Priority    = InParams.Priority;
            lSlot.StartOrder  = NextStartOrder++;
            lSlot.Volume      = Maths::Max(InParams.Volume, 0.f);
            lSlot.Fade        = 1.f;
            lSlot.Bus         = ResolveBus(InParams.Bus);
            lSlot.bSpatial    = InParams.bSpatial;
            lSlot.Position    = InParams.Position;
            lSlot.MinDistance = InParams.MinDistance;
            lSlot.MaxDistance = InParams.MaxDistance;
            lSlot.Attenuation = InParams.Attenuation;
            return SoundHandle::Make(lIndex, lSlot.Generation);
        }

        /**
         * Starts the reserved voice InSound with InClip.
         * @return False when it could not (the caller frees the voice)
         */
        bool StartVoice(const SoundHandle InSound, const AudioClipResource& InClip, const PlaySoundParams& InParams)
        {
            Slot* lSlot = Find(InSound);
            if (lSlot == nullptr || InClip.IsSilent())
            {
                return false;
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
                    return false;
                }

                lStream = MakeUnique<AudioStreamReader>(Move(lDecoder), AudioMixer::GetStreamWindowFrames(Mixer->GetBlockFrames()));
                lVoice.Source.Stream = lStream.get();
            }
            else
            {
                lVoice.Source.Frames     = lData.Frames.data();
                lVoice.Source.FrameCount = lData.Format.FrameCount;
            }

            lVoice.Volume       = lSlot->Volume;
            lVoice.Pitch        = InParams.Pitch;
            lVoice.Bus          = lSlot->Bus;
            lVoice.bLoop        = InParams.bLoop;
            lVoice.FadeInFrames = SecondsToFrames(InParams.FadeInSeconds, Settings.SampleRate);
            lVoice.bSpatial     = lSlot->bSpatial;
            lVoice.Position     = lSlot->Position;
            lVoice.MinDistance  = lSlot->MinDistance;
            lVoice.MaxDistance  = lSlot->MaxDistance;
            lVoice.Attenuation  = lSlot->Attenuation;

            AudioCommand lCommand;
            lCommand.Type   = EAudioCommand::Play;
            lCommand.Handle = InSound;
            lCommand.Voice  = lVoice;
            if (!Send(lCommand))
            {
                return false;
            }

            lSlot->bPending = false;
            lSlot->Data     = InClip.Data;
            lSlot->Stream   = Move(lStream);
            return true;
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
            lImpl.Settings.Buses     = lSettings->Buses;
        }

        lImpl.Settings.SampleRate  = Maths::Clamp(lImpl.Settings.SampleRate, 8000u, 192000u);
        lImpl.Settings.MaxVoices   = Maths::Max(lImpl.Settings.MaxVoices, 1u);
        lImpl.Settings.BlockFrames = Maths::Clamp(lImpl.Settings.BlockFrames, 32u, 4096u);
        AudioAPI::SetBackend(lImpl.Settings.Backend);

        AudioMixer::Desc lMix;
        lMix.SampleRate  = lImpl.Settings.SampleRate;
        lMix.MaxVoices   = lImpl.Settings.MaxVoices;
        lMix.BlockFrames = lImpl.Settings.BlockFrames;

        lImpl.BuildBuses(lImpl.Settings.Buses);
        for (const Impl::BusView& lBus : lImpl.Buses)
        {
            lMix.Buses.push_back(AudioBusDesc{ lBus.Id, lBus.Parent, lBus.Volume });
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
        lImpl.UpdatePending();

        const Uint32 lUnderruns = lImpl.Underruns.load(EMemoryOrder::Relaxed);
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
        lImpl.Pending.clear();

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

        const SoundHandle lHandle = lImpl.Reserve(InParams);
        if (lHandle.IsValid() && !lImpl.StartVoice(lHandle, InClip, InParams))
        {
            lImpl.Release(*lImpl.Find(lHandle));
            return SoundHandle{};
        }
        return lHandle;
    }

    SoundHandle AudioManager::Play(const ResourceRef<AudioClipResource>& InClip, const PlaySoundParams& InParams)
    {
        Impl& lImpl = *m_Impl;
        if (!lImpl.bStarted || !InClip.IsValid())
        {
            return SoundHandle{};
        }

        const EResourceState lState = InClip.GetManager()->GetState<AudioClipResource>(InClip.GetHandle());
        if (lState == EResourceState::Loaded)
        {
            return Play(*InClip.Get(), InParams);
        }

        if (lState != EResourceState::Loading)
        {
            return SoundHandle{};
        }

        // Started by Update once the clip is loaded.
        const SoundHandle lHandle = lImpl.Reserve(InParams);
        if (lHandle.IsValid())
        {
            lImpl.Find(lHandle)->bPending = true;
            lImpl.Pending.push_back(Impl::PendingPlay{ lHandle, InClip, InParams });
        }
        return lHandle;
    }

    Int32 AudioManager::SelectClip(const AudioClipDef& InDef, const OpaaxStringID InKey, PlaySoundParams& OutParams)
    {
        Impl& lImpl = *m_Impl;

        const Int32 lIndex = SelectAudioClip(InDef, lImpl.Selections[InKey], lImpl.Random);
        if (lIndex >= 0)
        {
            OutParams = MakePlaySoundParams(InDef, lIndex, lImpl.Random);
        }
        return lIndex;
    }

    void AudioManager::SetRandomSeed(const Uint64 InSeed)
    {
        m_Impl->Random.Seed(InSeed);
        m_Impl->Selections.clear();
    }

    void AudioManager::Stop(const SoundHandle InSound, const float InFadeOutSeconds)
    {
        if (m_Impl->FindPending(InSound) != nullptr)
        {
            m_Impl->CancelPending(InSound);
            return;
        }

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

        while (!lImpl.Pending.empty())
        {
            lImpl.CancelPending(lImpl.Pending.back().Handle);
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
            lSlot->Volume = Maths::Max(InVolume, 0.f);
            if (!lSlot->bPending)
            {
                m_Impl->Send(EAudioCommand::SetVolume, InSound, lSlot->Volume);
            }
        }
    }

    void AudioManager::FadeTo(const SoundHandle InSound, const float InVolume, const float InSeconds)
    {
        if (Impl::Slot* lSlot = m_Impl->FindPlaying(InSound))
        {
            lSlot->Fade = Maths::Max(InVolume, 0.f);
            const Uint32 lFrames = SecondsToFrames(InSeconds, m_Impl->Settings.SampleRate);

            if (Impl::PendingPlay* lPending = m_Impl->FindPending(InSound))
            {
                lPending->FadeTarget = lSlot->Fade;
                lPending->FadeFrames = lFrames;
                return;
            }
            m_Impl->Send(EAudioCommand::FadeVolume, InSound, lSlot->Fade, lFrames);
        }
    }

    void AudioManager::SetPitch(const SoundHandle InSound, const float InPitch)
    {
        if (Impl::PendingPlay* lPending = m_Impl->FindPending(InSound))
        {
            lPending->Params.Pitch = InPitch;
        }
        else if (m_Impl->Find(InSound) != nullptr)
        {
            m_Impl->Send(EAudioCommand::SetPitch, InSound, InPitch);
        }
    }

    void AudioManager::SetPosition(const SoundHandle InSound, const Vector2F InPosition)
    {
        Impl::Slot* lSlot = m_Impl->Find(InSound);
        if (lSlot != nullptr && lSlot->Position != InPosition)
        {
            lSlot->Position = InPosition;
            if (lSlot->bPending)
            {
                return;
            }

            AudioCommand lCommand;
            lCommand.Type     = EAudioCommand::SetPosition;
            lCommand.Handle   = InSound;
            lCommand.Position = InPosition;
            m_Impl->Send(lCommand);
        }
    }

    void AudioManager::SetPaused(const SoundHandle InSound, const bool bInPaused)
    {
        if (Impl::PendingPlay* lPending = m_Impl->FindPending(InSound))
        {
            lPending->bPaused = bInPaused;
        }
        else if (m_Impl->Find(InSound) != nullptr)
        {
            m_Impl->Send(EAudioCommand::SetPaused, InSound, 0.f, 0, bInPaused);
        }
    }

    // =========================================================================
    // Mix
    // =========================================================================
    bool AudioManager::HasBus(const OpaaxStringID InBus) const
    {
        return m_Impl->BusIndices.contains(InBus);
    }

    void AudioManager::SetBusVolume(const OpaaxStringID InBus, const float InVolume)
    {
        const Int32 lBus = m_Impl->FindBus(InBus);
        if (lBus >= 0)
        {
            m_Impl->Buses[lBus].Volume = Maths::Max(InVolume, 0.f);
            m_Impl->SendBus(EAudioCommand::SetBusVolume, static_cast<Uint16>(lBus), m_Impl->Buses[lBus].Volume);
        }
    }

    float AudioManager::GetBusVolume(const OpaaxStringID InBus) const
    {
        const auto lFound = m_Impl->BusIndices.find(InBus);
        return lFound != m_Impl->BusIndices.end() ? m_Impl->Buses[lFound->second].Volume : 0.f;
    }

    void AudioManager::FadeBus(const OpaaxStringID InBus, const float InVolume, const float InSeconds)
    {
        const Int32 lBus = m_Impl->FindBus(InBus);
        if (lBus >= 0)
        {
            m_Impl->Buses[lBus].Fade = Maths::Max(InVolume, 0.f);
            m_Impl->SendBus(EAudioCommand::FadeBus, static_cast<Uint16>(lBus), m_Impl->Buses[lBus].Fade,
                            SecondsToFrames(InSeconds, m_Impl->Settings.SampleRate));
        }
    }

    void AudioManager::SetBusMuted(const OpaaxStringID InBus, const bool bInMuted)
    {
        const Int32 lBus = m_Impl->FindBus(InBus);
        if (lBus >= 0)
        {
            m_Impl->Buses[lBus].bMuted = bInMuted;
            m_Impl->SendBus(EAudioCommand::SetBusMute, static_cast<Uint16>(lBus), 0.f, 0, bInMuted);
        }
    }

    void AudioManager::SetBusSolo(const OpaaxStringID InBus, const bool bInSolo)
    {
        const Int32 lBus = m_Impl->FindBus(InBus);
        if (lBus >= 0)
        {
            m_Impl->SendBus(EAudioCommand::SetBusSolo, static_cast<Uint16>(lBus), 0.f, 0, bInSolo);
        }
    }

    void AudioManager::SetMasterVolume(const float InVolume)
    {
        SetBusVolume(AudioMasterBus(), InVolume);
    }

    float AudioManager::GetMasterVolume() const
    {
        return GetBusVolume(AudioMasterBus());
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

        // A bus added since Startup is built at the next start.
        for (const AudioBusSettings& lBus : InSettings.Buses)
        {
            if (HasBus(lBus.Name))
            {
                SetBusVolume(lBus.Name, lBus.Volume);
            }
        }
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
        return static_cast<Uint64>(Algo::CountIf(m_Impl->Slots,
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
            const Uint32 lFrames = static_cast<Uint32>(Maths::Min<Uint64>(InFrameCount - lDone, 1u << 20));
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
        return m_Impl->Underruns.load(EMemoryOrder::Relaxed);
    }
}
