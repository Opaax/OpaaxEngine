#pragma once

#include "Audio/IAudioDevice.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // MiniaudioDevice — IAudioDevice over miniaudio's playback device (the system's default output).
    // =============================================================================
    class MiniaudioDevice final : public IAudioDevice
    {
    public:
        MiniaudioDevice();
        ~MiniaudioDevice() override;

        MiniaudioDevice(const MiniaudioDevice&)            = delete;
        MiniaudioDevice& operator=(const MiniaudioDevice&) = delete;

        //~Begin IAudioDevice interface
        bool        Start(const AudioDeviceDesc& InDesc, AudioDeviceCallback InCallback, void* InUser) override;
        void        Stop() override;
        Uint32      GetPeriodFrames() const noexcept override;
        const char* GetName() const noexcept override { return m_Name.CStr(); }
        //~End IAudioDevice interface

    private:
        struct Impl;
        TUniquePtr<Impl> m_Impl;
        OpaaxString      m_Name;
    };
}
