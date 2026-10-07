#pragma once

#include "Audio/AudioTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Subsystems/EngineSubsystem.h"

namespace Opaax
{
    struct AudioClipResource;
    struct AudioSettings;

    // =============================================================================
    // AudioManager — mixes every sound and owns the audio device (miniaudio's engine; nothing
    //   outside Audio/ sees miniaudio). An engine subsystem: a sound is not tied to a world unless
    //   the world stops it (AudioSubsystem does when it ends).
    //
    //   Without an audio device (a server, CI, Audio.bEnabled off) it runs headless: sounds play
    //   silently and advance with Update, so they still end on time.
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
            bool   bHeadless  = true;
            Uint32 SampleRate = 48000;
            Uint32 Channels   = 2;
            Uint32 MaxVoices  = 64;
        };

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /** The engine's: the settings come from the engine config at Startup. */
        AudioManager();

        /** A manager with its own settings (tests, tools). */
        explicit AudioManager(const Desc& InDesc);

        ~AudioManager() override;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface
        bool Startup() override;

        /** Releases finished sounds; a headless mix advances by InDeltaTime. */
        void Update(double InDeltaTime) override;

        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Sounds
        // =============================================================================
    public:
        /**
         * Starts InClip. The clip's bytes are shared with the sound, so the clip may be unloaded
         * while it plays.
         * @return Invalid when the clip is silent, the voice limit is reached, or audio is not started
         */
        SoundHandle Play(const AudioClipResource& InClip, const PlaySoundParams& InParams = {});

        void Stop(SoundHandle InSound);
        void StopAll();

        /** True from Play until the sound ends or is stopped. */
        bool IsPlaying(SoundHandle InSound) const;

        void SetVolume(SoundHandle InSound, float InVolume);
        void SetPitch(SoundHandle InSound, float InPitch);

        /** For a spatial sound: where it is in the world. */
        void SetPosition(SoundHandle InSound, Vector2F InPosition);

        void SetPaused(SoundHandle InSound, bool bInPaused);

        // =============================================================================
        // Mix
        // =============================================================================
    public:
        void  SetBusVolume(EAudioBus InBus, float InVolume);
        float GetBusVolume(EAudioBus InBus) const;

        void  SetMasterVolume(float InVolume);
        float GetMasterVolume() const;

        /** Where spatial sounds are heard from (usually the camera or the player). */
        void     SetListenerPosition(Vector2F InPosition);
        Vector2F GetListenerPosition() const;

        /** Volumes from the engine config (startup, and when the config changes). */
        void ApplySettings(const AudioSettings& InSettings);

        // =============================================================================
        // State
        // =============================================================================
    public:
        bool IsStarted() const noexcept;

        /** No audio device: sounds advance with Update and are not heard. */
        bool IsHeadless() const noexcept;

        /** Sounds currently held (playing or paused). */
        Uint64 GetVoiceCount() const noexcept;

        /**
         * Headless only: mixes InFrameCount frames (interleaved, GetChannels() floats per frame) into
         * OutFrames, advancing every sound. For tests and offline capture.
         * @return Frames written, 0 when not headless
         */
        Uint64 RenderFrames(float* OutFrames, Uint64 InFrameCount);

        Uint32 GetSampleRate() const noexcept;
        Uint32 GetChannels() const noexcept;

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
