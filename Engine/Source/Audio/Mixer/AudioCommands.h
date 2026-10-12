#pragma once

#include "Audio/AudioTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class AudioStreamReader;

    // =============================================================================
    // AudioVoiceSource — what a voice reads: a whole decoded clip, or a stream decoded while it
    //   plays. The game thread keeps both alive until the voice reports it ended.
    // =============================================================================
    struct AudioVoiceSource
    {
        /** Decoded: interleaved frames, Channels floats each. */
        const float* Frames     = nullptr;
        Uint64       FrameCount = 0;

        /** Streamed: read through the stream, Frames is null. */
        AudioStreamReader* Stream = nullptr;

        Uint32 Channels   = 0;   // 1 or 2
        Uint32 SampleRate = 0;

        bool IsEmpty() const noexcept { return Stream == nullptr && (Frames == nullptr || FrameCount == 0); }
    };

    // =============================================================================
    // AudioVoiceDesc — how a voice starts.
    // =============================================================================
    struct AudioVoiceDesc
    {
        AudioVoiceSource Source;

        float  Volume       = 1.f;
        float  Pitch        = 1.f;
        float  Pan          = 0.f;   // -1 left to 1 right; spatial voices are panned by position
        Uint16 Bus          = 0;     // bus index in the mixer
        bool   bLoop        = false;
        Uint32 FadeInFrames = 0;

        bool              bSpatial    = false;
        Vector2F          Position    = { 0.f, 0.f };
        float             MinDistance = 100.f;
        float             MaxDistance = 1500.f;
        EAudioAttenuation Attenuation = EAudioAttenuation::Linear;
    };

    // =============================================================================
    // EAudioCommand / AudioCommand — what the game thread asks of the mixer. Fade lengths are in
    //   frames at the mixer's rate.
    // =============================================================================
    enum class EAudioCommand : Uint8
    {
        None,
        Play,           // Handle, Voice
        Stop,           // Handle, Frames (fade-out; 0 still ramps over one block)
        StopAll,        // Frames
        SetPaused,      // Handle, bFlag
        SetVolume,      // Handle, Value
        FadeVolume,     // Handle, Value (target), Frames
        SetPitch,       // Handle, Value
        SetPosition,    // Handle, Position
        SetListener,    // Position
        SetBusVolume,   // Bus, Value
        FadeBus,        // Bus, Value (target), Frames
        SetBusMute,     // Bus, bFlag
        SetBusSolo      // Bus, bFlag
    };

    struct AudioCommand
    {
        EAudioCommand Type     = EAudioCommand::None;
        SoundHandle   Handle;
        float         Value    = 0.f;
        Uint32        Frames   = 0;
        Vector2F      Position = { 0.f, 0.f };
        Uint16        Bus      = 0;
        bool          bFlag    = false;

        AudioVoiceDesc Voice;   // Play only
    };

    // =============================================================================
    // EAudioEvent / AudioEvent — what the mixer tells the game thread.
    // =============================================================================
    enum class EAudioEvent : Uint8
    {
        /** The voice is free: its sound ended, was stopped, or was replaced. Its source may go. */
        VoiceEnded
    };

    struct AudioEvent
    {
        EAudioEvent Type = EAudioEvent::VoiceEnded;
        SoundHandle Handle;
    };
}
