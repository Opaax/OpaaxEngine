#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // AudioDeviceDesc — what the engine plays: float frames, interleaved.
    // =============================================================================
    struct AudioDeviceDesc
    {
        Uint32 SampleRate   = 48000;
        Uint32 Channels     = 2;
        Uint32 PeriodFrames = 256;   // frames asked per callback, a hint the device may round
    };

    /**
     * Fills OutFrames with InFrameCount frames (AudioDeviceDesc::Channels floats each). Called on the
     * device's own thread: it must not block, lock or allocate.
     */
    using AudioDeviceCallback = void (*)(void* InUser, float* OutFrames, Uint32 InFrameCount);

    // =============================================================================
    // IAudioDevice — the sound card output. Made by the audio backend (AudioAPI::CreateDevice); it
    //   only pulls frames through the callback, converting them to what the hardware plays.
    // =============================================================================
    class IAudioDevice
    {
    public:
        virtual ~IAudioDevice() = default;

        /**
         * Opens the default output and starts calling InCallback.
         * @return False when no device can be opened
         */
        virtual bool Start(const AudioDeviceDesc& InDesc, AudioDeviceCallback InCallback, void* InUser) = 0;

        /** Stops the callbacks; none runs after it returns. */
        virtual void Stop() = 0;

        /** Frames the device actually asks per callback, once started. */
        virtual Uint32 GetPeriodFrames() const noexcept = 0;

        /** The device's name, for the log. */
        virtual const char* GetName() const noexcept = 0;
    };
}
