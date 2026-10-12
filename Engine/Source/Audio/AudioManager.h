#pragma once

#include "Audio/AudioBackend.h"
#include "Audio/AudioBusSettings.h"
#include "Audio/AudioTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Resources/ResourceRef.hpp"

namespace Opaax
{
    class IAudioDevice;
    struct AudioClipDef;
    struct AudioClipResource;
    struct AudioSettings;

    // =============================================================================
    // AudioManager — the game's door to audio: plays, stops and fades sounds through handles, and
    //   sets the buses and the listener. An engine subsystem: a sound is not tied to a world unless
    //   the world stops it (AudioSubsystem does when it ends).
    //
    //   The game thread never touches the mix: each call is a command to the audio thread, which
    //   mixes (AudioMixer) ahead into a ring buffer the backend's device plays. Handles are reserved
    //   here, so they are usable at once; IsPlaying reads this side's view of the voices, which
    //   changes at once on Play and Stop, and one frame late when a sound ends by itself.
    //
    //   Without an audio device (a server, CI, Audio.bEnabled off) it runs headless: no thread, the
    //   game thread mixes as time passes (Update), so sounds still end on time.
    // =============================================================================
    class AudioManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(AudioManager)

        /** How a manager built outside the engine (tests, tools) mixes. */
        struct Desc
        {
            bool          bHeadless   = true;
            EAudioBackend Backend     = EAudioBackend::Miniaudio;
            Uint32        SampleRate  = 48000;
            Uint32        MaxVoices   = 64;
            Uint32        BlockFrames = 256;

            /** The buses under the master. */
            TDynArray<AudioBusSettings> Buses = DefaultAudioBuses();
        };

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /** The engine's: the settings come from the engine config at Startup. */
        AudioManager();

        /** A manager with its own settings (tests, tools). */
        explicit AudioManager(const Desc& InDesc);

        /** A manager that plays to InDevice instead of the backend's (tests: a device that records). */
        AudioManager(const Desc& InDesc, TUniquePtr<IAudioDevice> InDevice);

        ~AudioManager() override;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface
        bool Startup() override;

        /** Frees the voices that ended; a headless mix advances by InDeltaTime. */
        void Update(double InDeltaTime) override;

        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Sounds
        // =============================================================================
    public:
        /**
         * Starts InClip. The clip's data is shared with the sound, so the clip may be unloaded while
         * it plays. When every voice is busy, the one least worth keeping is replaced: one already
         * stopping, else the lowest priority, then the quietest.
         * @return Invalid when the clip is silent, audio is not started, or every voice plays
         *   something of a higher priority
         */
        SoundHandle Play(const AudioClipResource& InClip, const PlaySoundParams& InParams = {});

        /**
         * Starts InClip, or, while it is still loading, starts it as soon as it is loaded: the handle
         * is playing from now, and every call on it is kept until then.
         * @return Invalid when the clip failed to load, or as Play above
         */
        SoundHandle Play(const ResourceRef<AudioClipResource>& InClip, const PlaySoundParams& InParams = {});

        /**
         * Chooses which clip of InDef plays now and how: its bus, volume and pitch (with the spread
         * drawn now), loop and space. Game thread, seeded random (SetRandomSeed).
         * @param InKey Keeps the definition's choices between plays (its path): no repeat, the order
         * @return The clip's index in InDef.Clips, -1 when it has none
         */
        Int32 SelectClip(const AudioClipDef& InDef, OpaaxStringID InKey, PlaySoundParams& OutParams);

        /** Same seed, same choices: variations, volume and pitch spread. */
        void SetRandomSeed(Uint64 InSeed);

        /**
         * Stops a sound, fading it out over InFadeOutSeconds. It no longer counts as playing.
         * 0 still fades over a few milliseconds, so it never clicks.
         */
        void Stop(SoundHandle InSound, float InFadeOutSeconds = 0.f);
        void StopAll(float InFadeOutSeconds = 0.f);

        /** True from Play until the sound ends or is stopped. */
        bool IsPlaying(SoundHandle InSound) const;

        void SetVolume(SoundHandle InSound, float InVolume);

        /** Moves the sound's fade to InVolume over InSeconds (on top of its volume). */
        void FadeTo(SoundHandle InSound, float InVolume, float InSeconds);

        void SetPitch(SoundHandle InSound, float InPitch);

        /** For a spatial sound: where it is in the world. */
        void SetPosition(SoundHandle InSound, Vector2F InPosition);

        void SetPaused(SoundHandle InSound, bool bInPaused);

        // =============================================================================
        // Buses — named as in the engine config ("Master", "Music", "SFX"...). A name that is not
        //   a bus is logged once and does nothing.
        // =============================================================================
    public:
        bool HasBus(OpaaxStringID InBus) const;

        /** The bus's own volume (the settings' one). */
        void  SetBusVolume(OpaaxStringID InBus, float InVolume);
        float GetBusVolume(OpaaxStringID InBus) const;

        /**
         * Moves the bus's fade to InVolume over InSeconds, on top of its volume: a cutscene fades the
         * music out without touching the volume the player chose.
         */
        void FadeBus(OpaaxStringID InBus, float InVolume, float InSeconds);

        void SetBusMuted(OpaaxStringID InBus, bool bInMuted);

        /** While a bus is soloed, only it and the buses under it are heard. */
        void SetBusSolo(OpaaxStringID InBus, bool bInSolo);

        void  SetMasterVolume(float InVolume);
        float GetMasterVolume() const;

        // =============================================================================
        // Listener and settings
        // =============================================================================
    public:
        /** Where spatial sounds are heard from (usually the camera or the player). */
        void     SetListenerPosition(Vector2F InPosition);
        Vector2F GetListenerPosition() const;

        /** Volumes from the engine config (startup, and when the config changes). The bus tree is built at Startup. */
        void ApplySettings(const AudioSettings& InSettings);

        // =============================================================================
        // State
        // =============================================================================
    public:
        bool IsStarted() const noexcept;

        /** No audio device: sounds advance with Update and are not heard. */
        bool IsHeadless() const noexcept;

        /** Sounds playing or paused (not the ones fading out after a stop). */
        Uint64 GetVoiceCount() const noexcept;

        /**
         * Headless only: mixes InFrameCount frames (interleaved, GetChannels() floats per frame) into
         * OutFrames, advancing every sound. For tests and offline capture.
         * @return Frames written, 0 when not headless
         */
        Uint64 RenderFrames(float* OutFrames, Uint64 InFrameCount);

        Uint32 GetSampleRate() const noexcept;
        Uint32 GetChannels() const noexcept;

        /** Times the device found nothing mixed to play (heard as a gap). */
        Uint32 GetUnderrunCount() const noexcept;

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** Re-reads the volumes (the engine's manager only). */
        void OnEngineConfigChanged();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct Impl;
        TUniquePtr<Impl> m_Impl;
    };
}
