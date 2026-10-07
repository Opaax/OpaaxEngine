#include "Audio/AudioManager.h"

// The miniaudio implementation is compiled here, once.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Audio/AudioClipResource.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/Config/EngineConfigData.h"

namespace Opaax
{
    namespace
    {
        /** Frames mixed per call when a headless mix advances. */
        constexpr Uint64 HEADLESS_CHUNK_FRAMES = 1024;

        /** A spatial sound's max distance is kept above its min distance by at least this. */
        constexpr float MIN_DISTANCE_SPAN = 1.f;
    }

    // =========================================================================
    // Impl — every miniaudio object, behind the header.
    // =========================================================================
    struct AudioManager::Impl
    {
        /** One playing sound. Allocated alone: miniaudio keeps pointers to the decoder and sound. */
        struct Voice
        {
            Uint64     Id = 0;
            ma_decoder Decoder{};
            ma_sound   Sound{};
            bool       bLoop         = false;
            bool       bDecoderReady = false;
            bool       bSoundReady   = false;

            /** Read by the decoder while the sound plays. */
            TSharedPtr<const TDynArray<Uint8>> Bytes;
        };

        Desc Settings;

        /** The engine's manager reads its settings from the engine config. */
        bool bFromConfig = false;

        ma_engine      Engine{};
        ma_sound_group Buses[AUDIO_BUS_COUNT]{};
        bool           bEngineReady = false;
        bool           bBusReady[AUDIO_BUS_COUNT]{};
        bool           bHeadless    = true;

        TDynArray<TUniquePtr<Voice>> Voices;
        Uint64 NextId            = 1;
        bool   bWarnedVoiceLimit = false;

        float    MasterVolume = 1.f;
        float    BusVolumes[AUDIO_BUS_COUNT] = { 1.f, 1.f, 1.f, 1.f };
        Vector2F ListenerPosition = { 0.f, 0.f };

        /** Headless: the fraction of a frame carried to the next Update, and the mix buffer. */
        double           PendingFrames = 0.0;
        TDynArray<float> Scratch;

        Config_Engine* EngineConfig = nullptr;

        Voice* Find(const SoundHandle InSound) const
        {
            if (!InSound.IsValid())
            {
                return nullptr;
            }

            for (const TUniquePtr<Voice>& lVoice : Voices)
            {
                if (lVoice->Id == InSound.Id) { return lVoice.get(); }
            }
            return nullptr;
        }

        /** The sound first (it reads the decoder), then the decoder. */
        static void Release(Voice& InVoice)
        {
            if (InVoice.bSoundReady)
            {
                ma_sound_uninit(&InVoice.Sound);
                InVoice.bSoundReady = false;
            }

            if (InVoice.bDecoderReady)
            {
                ma_decoder_uninit(&InVoice.Decoder);
                InVoice.bDecoderReady = false;
            }

            InVoice.Bytes.reset();
        }

        void Mix(Uint64 InFrames)
        {
            const Uint64 lChannels = ma_engine_get_channels(&Engine);
            Scratch.resize(static_cast<size_t>(HEADLESS_CHUNK_FRAMES * lChannels));

            while (InFrames > 0)
            {
                const Uint64 lChunk = std::min(InFrames, HEADLESS_CHUNK_FRAMES);
                ma_engine_read_pcm_frames(&Engine, Scratch.data(), lChunk, nullptr);
                InFrames -= lChunk;
            }
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

    AudioManager::~AudioManager()
    {
        Shutdown();
    }

    // =========================================================================
    // ISubsystem
    // =========================================================================
    bool AudioManager::Startup()
    {
        if (m_Impl->bEngineReady)
        {
            return true;
        }

        const AudioSettings* lSettings = nullptr;
        if (m_Impl->bFromConfig)
        {
            m_Impl->EngineConfig = &OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>();
            lSettings = &m_Impl->EngineConfig->GetData().Audio;

            m_Impl->Settings.bHeadless = !lSettings->bEnabled;
            m_Impl->Settings.MaxVoices = lSettings->MaxVoices;
        }

        // A device first, unless headless was asked for. Without one, a headless mix.
        bool bStarted = false;
        if (!m_Impl->Settings.bHeadless)
        {
            const ma_engine_config lConfig = ma_engine_config_init();
            bStarted = ma_engine_init(&lConfig, &m_Impl->Engine) == MA_SUCCESS;

            if (!bStarted)
            {
                OPAAX_LOG(LogAudio, Warn, "No audio device could be opened: sounds will play silently");
            }
        }

        m_Impl->bHeadless = !bStarted;
        if (!bStarted)
        {
            ma_engine_config lConfig = ma_engine_config_init();
            lConfig.noDevice   = MA_TRUE;
            lConfig.channels   = std::max(m_Impl->Settings.Channels, 1u);
            lConfig.sampleRate = std::max(m_Impl->Settings.SampleRate, 8000u);

            if (ma_engine_init(&lConfig, &m_Impl->Engine) != MA_SUCCESS)
            {
                // Audio is optional: the engine runs on without it.
                OPAAX_LOG(LogAudio, Error, "The audio mixer could not start: no sound will play");
                return true;
            }
        }

        m_Impl->bEngineReady = true;

        for (Uint8 lBus = 0; lBus < AUDIO_BUS_COUNT; ++lBus)
        {
            m_Impl->bBusReady[lBus] = ma_sound_group_init(&m_Impl->Engine, 0, nullptr, &m_Impl->Buses[lBus]) == MA_SUCCESS;
            if (!m_Impl->bBusReady[lBus])
            {
                OPAAX_LOG(LogAudio, Error, "Audio bus '{}' could not be created: its sounds play on the master",
                          ToString(static_cast<EAudioBus>(lBus)));
            }
        }

        if (lSettings != nullptr)
        {
            ApplySettings(*lSettings);

            // Volumes follow the settings while the game runs.
            m_Impl->EngineConfig->OnChanged().AddMember(this, &AudioManager::OnEngineConfigChanged);
        }

        OPAAX_LOG(LogAudio, Info, "Audio started — {} Hz, {} channel(s){}", GetSampleRate(), GetChannels(),
                  m_Impl->bHeadless ? ", headless (not heard)" : "");
        return true;
    }

    void AudioManager::Update(const double InDeltaTime)
    {
        if (!m_Impl->bEngineReady)
        {
            return;
        }

        // Nothing pulls a headless mix: it advances by the frame's time, so sounds end on time.
        if (m_Impl->bHeadless && !m_Impl->Voices.empty() && InDeltaTime > 0.0)
        {
            m_Impl->PendingFrames += InDeltaTime * static_cast<double>(GetSampleRate());
            const Uint64 lFrames = static_cast<Uint64>(m_Impl->PendingFrames);
            m_Impl->PendingFrames -= static_cast<double>(lFrames);

            m_Impl->Mix(lFrames);
        }

        // Release the sounds that ended (a looping sound never does).
        TDynArray<TUniquePtr<Impl::Voice>>& lVoices = m_Impl->Voices;
        lVoices.erase(std::remove_if(lVoices.begin(), lVoices.end(),
                                     [](const TUniquePtr<Impl::Voice>& InVoice)
                                     {
                                         if (InVoice->bLoop || !ma_sound_at_end(&InVoice->Sound))
                                         {
                                             return false;
                                         }

                                         Impl::Release(*InVoice);
                                         return true;
                                     }),
                      lVoices.end());
    }

    void AudioManager::Shutdown()
    {
        if (!m_Impl || !m_Impl->bEngineReady)
        {
            return;
        }

        if (m_Impl->EngineConfig != nullptr)
        {
            m_Impl->EngineConfig->OnChanged().RemoveAll(this);
            m_Impl->EngineConfig = nullptr;
        }

        StopAll();

        for (Uint8 lBus = 0; lBus < AUDIO_BUS_COUNT; ++lBus)
        {
            if (m_Impl->bBusReady[lBus])
            {
                ma_sound_group_uninit(&m_Impl->Buses[lBus]);
                m_Impl->bBusReady[lBus] = false;
            }
        }

        ma_engine_uninit(&m_Impl->Engine);
        m_Impl->bEngineReady = false;

        OPAAX_LOG(LogAudio, Trace, "Audio stopped");
    }

    // =========================================================================
    // Sounds
    // =========================================================================
    SoundHandle AudioManager::Play(const AudioClipResource& InClip, const PlaySoundParams& InParams)
    {
        if (!m_Impl->bEngineReady || InClip.IsSilent())
        {
            return SoundHandle{};
        }

        if (m_Impl->Voices.size() >= m_Impl->Settings.MaxVoices)
        {
            if (!m_Impl->bWarnedVoiceLimit)
            {
                m_Impl->bWarnedVoiceLimit = true;
                OPAAX_LOG(LogAudio, Warn, "{} sounds are already playing (Audio.MaxVoices): the new ones are dropped",
                          m_Impl->Settings.MaxVoices);
            }
            return SoundHandle{};
        }

        TUniquePtr<Impl::Voice> lVoice = MakeUnique<Impl::Voice>();
        lVoice->Bytes = InClip.Encoded;
        lVoice->bLoop = InParams.bLoop;

        // The clip's own format: the mixer converts channels and sample rate.
        const ma_decoder_config lDecoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decoder_init_memory(lVoice->Bytes->data(), lVoice->Bytes->size(), &lDecoderConfig, &lVoice->Decoder) != MA_SUCCESS)
        {
            OPAAX_LOG(LogAudio, Error, "A sound could not start: its clip does not decode");
            return SoundHandle{};
        }
        lVoice->bDecoderReady = true;

        const Uint8     lBus   = static_cast<Uint8>(InParams.Bus);
        ma_sound_group* lGroup = (lBus < AUDIO_BUS_COUNT && m_Impl->bBusReady[lBus]) ? &m_Impl->Buses[lBus] : nullptr;
        const ma_uint32 lFlags = InParams.bSpatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;

        if (ma_sound_init_from_data_source(&m_Impl->Engine, &lVoice->Decoder, lFlags, lGroup, &lVoice->Sound) != MA_SUCCESS)
        {
            OPAAX_LOG(LogAudio, Error, "A sound could not start: the mixer refused it");
            Impl::Release(*lVoice);
            return SoundHandle{};
        }
        lVoice->bSoundReady = true;

        ma_sound* lSound = &lVoice->Sound;
        ma_sound_set_volume(lSound, std::max(InParams.Volume, 0.f));
        ma_sound_set_pitch(lSound, std::max(InParams.Pitch, 0.01f));
        ma_sound_set_looping(lSound, InParams.bLoop ? MA_TRUE : MA_FALSE);

        if (InParams.bSpatial)
        {
            // 2D: everything on the z = 0 plane, the listener looking down -z with y up.
            ma_sound_set_position(lSound, InParams.Position.x, InParams.Position.y, 0.f);
            ma_sound_set_attenuation_model(lSound, ma_attenuation_model_linear);
            ma_sound_set_min_distance(lSound, std::max(InParams.MinDistance, 0.f));
            ma_sound_set_max_distance(lSound, std::max(InParams.MaxDistance, std::max(InParams.MinDistance, 0.f) + MIN_DISTANCE_SPAN));
            ma_sound_set_doppler_factor(lSound, 0.f);
        }

        if (ma_sound_start(lSound) != MA_SUCCESS)
        {
            OPAAX_LOG(LogAudio, Error, "A sound could not start");
            Impl::Release(*lVoice);
            return SoundHandle{};
        }

        lVoice->Id = m_Impl->NextId++;
        const SoundHandle lHandle{ lVoice->Id };
        m_Impl->Voices.push_back(Move(lVoice));
        return lHandle;
    }

    void AudioManager::Stop(const SoundHandle InSound)
    {
        TDynArray<TUniquePtr<Impl::Voice>>& lVoices = m_Impl->Voices;
        for (auto lIt = lVoices.begin(); lIt != lVoices.end(); ++lIt)
        {
            if ((*lIt)->Id == InSound.Id)
            {
                Impl::Release(**lIt);
                lVoices.erase(lIt);
                return;
            }
        }
    }

    void AudioManager::StopAll()
    {
        for (TUniquePtr<Impl::Voice>& lVoice : m_Impl->Voices)
        {
            Impl::Release(*lVoice);
        }
        m_Impl->Voices.clear();
    }

    bool AudioManager::IsPlaying(const SoundHandle InSound) const
    {
        const Impl::Voice* lVoice = m_Impl->Find(InSound);
        return lVoice != nullptr && (lVoice->bLoop || !ma_sound_at_end(&lVoice->Sound));
    }

    void AudioManager::SetVolume(const SoundHandle InSound, const float InVolume)
    {
        if (Impl::Voice* lVoice = m_Impl->Find(InSound))
        {
            ma_sound_set_volume(&lVoice->Sound, std::max(InVolume, 0.f));
        }
    }

    void AudioManager::SetPitch(const SoundHandle InSound, const float InPitch)
    {
        if (Impl::Voice* lVoice = m_Impl->Find(InSound))
        {
            ma_sound_set_pitch(&lVoice->Sound, std::max(InPitch, 0.01f));
        }
    }

    void AudioManager::SetPosition(const SoundHandle InSound, const Vector2F InPosition)
    {
        if (Impl::Voice* lVoice = m_Impl->Find(InSound))
        {
            ma_sound_set_position(&lVoice->Sound, InPosition.x, InPosition.y, 0.f);
        }
    }

    void AudioManager::SetPaused(const SoundHandle InSound, const bool bInPaused)
    {
        if (Impl::Voice* lVoice = m_Impl->Find(InSound))
        {
            if (bInPaused) { ma_sound_stop(&lVoice->Sound); }
            else           { ma_sound_start(&lVoice->Sound); }
        }
    }

    // =========================================================================
    // Mix
    // =========================================================================
    void AudioManager::SetBusVolume(const EAudioBus InBus, const float InVolume)
    {
        const Uint8 lBus = static_cast<Uint8>(InBus);
        if (lBus >= AUDIO_BUS_COUNT)
        {
            return;
        }

        m_Impl->BusVolumes[lBus] = std::max(InVolume, 0.f);
        if (m_Impl->bBusReady[lBus])
        {
            ma_sound_group_set_volume(&m_Impl->Buses[lBus], m_Impl->BusVolumes[lBus]);
        }
    }

    float AudioManager::GetBusVolume(const EAudioBus InBus) const
    {
        const Uint8 lBus = static_cast<Uint8>(InBus);
        return lBus < AUDIO_BUS_COUNT ? m_Impl->BusVolumes[lBus] : 0.f;
    }

    void AudioManager::SetMasterVolume(const float InVolume)
    {
        m_Impl->MasterVolume = std::max(InVolume, 0.f);
        if (m_Impl->bEngineReady)
        {
            ma_engine_set_volume(&m_Impl->Engine, m_Impl->MasterVolume);
        }
    }

    float AudioManager::GetMasterVolume() const
    {
        return m_Impl->MasterVolume;
    }

    void AudioManager::SetListenerPosition(const Vector2F InPosition)
    {
        m_Impl->ListenerPosition = InPosition;
        if (m_Impl->bEngineReady)
        {
            ma_engine_listener_set_position(&m_Impl->Engine, 0, InPosition.x, InPosition.y, 0.f);
        }
    }

    Vector2F AudioManager::GetListenerPosition() const
    {
        return m_Impl->ListenerPosition;
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
        return m_Impl->bEngineReady;
    }

    bool AudioManager::IsHeadless() const noexcept
    {
        return m_Impl->bHeadless;
    }

    Uint64 AudioManager::GetVoiceCount() const noexcept
    {
        return static_cast<Uint64>(m_Impl->Voices.size());
    }

    Uint64 AudioManager::RenderFrames(float* OutFrames, const Uint64 InFrameCount)
    {
        if (!m_Impl->bEngineReady || !m_Impl->bHeadless || OutFrames == nullptr)
        {
            return 0;
        }

        ma_uint64 lRead = 0;
        ma_engine_read_pcm_frames(&m_Impl->Engine, OutFrames, InFrameCount, &lRead);
        return static_cast<Uint64>(lRead);
    }

    Uint32 AudioManager::GetSampleRate() const noexcept
    {
        return m_Impl->bEngineReady ? ma_engine_get_sample_rate(&m_Impl->Engine) : 0;
    }

    Uint32 AudioManager::GetChannels() const noexcept
    {
        return m_Impl->bEngineReady ? ma_engine_get_channels(&m_Impl->Engine) : 0;
    }
}
