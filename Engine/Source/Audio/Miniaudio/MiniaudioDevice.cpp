#include "Audio/Miniaudio/MiniaudioDevice.h"

// The miniaudio implementation is compiled here, once.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace Opaax
{
    struct MiniaudioDevice::Impl
    {
        ma_device           Device{};
        bool                bReady   = false;
        AudioDeviceCallback Callback = nullptr;
        void*               User     = nullptr;

        /** miniaudio's device thread. */
        static void OnData(ma_device* InDevice, void* OutFrames, const void* /*InInput*/, const ma_uint32 InFrameCount)
        {
            const Impl* lSelf = static_cast<const Impl*>(InDevice->pUserData);
            lSelf->Callback(lSelf->User, static_cast<float*>(OutFrames), static_cast<Uint32>(InFrameCount));
        }
    };

    MiniaudioDevice::MiniaudioDevice()
        : m_Impl(MakeUnique<Impl>())
    {
    }

    MiniaudioDevice::~MiniaudioDevice()
    {
        Stop();
    }

    bool MiniaudioDevice::Start(const AudioDeviceDesc& InDesc, const AudioDeviceCallback InCallback, void* InUser)
    {
        if (m_Impl->bReady || InCallback == nullptr)
        {
            return false;
        }

        m_Impl->Callback = InCallback;
        m_Impl->User     = InUser;

        // The engine's format; miniaudio converts to the hardware's.
        ma_device_config lConfig          = ma_device_config_init(ma_device_type_playback);
        lConfig.playback.format           = ma_format_f32;
        lConfig.playback.channels         = InDesc.Channels;
        lConfig.sampleRate                = InDesc.SampleRate;
        lConfig.periodSizeInFrames        = InDesc.PeriodFrames;
        lConfig.dataCallback              = &Impl::OnData;
        lConfig.pUserData                 = m_Impl.get();
        lConfig.noPreSilencedOutputBuffer = MA_TRUE;   // the callback writes every frame

        if (ma_device_init(nullptr, &lConfig, &m_Impl->Device) != MA_SUCCESS)
        {
            return false;
        }

        if (ma_device_start(&m_Impl->Device) != MA_SUCCESS)
        {
            ma_device_uninit(&m_Impl->Device);
            return false;
        }

        m_Impl->bReady = true;

        char lName[256] = {};
        ma_device_get_name(&m_Impl->Device, ma_device_type_playback, lName, sizeof(lName), nullptr);
        m_Name = OpaaxString(lName);
        return true;
    }

    void MiniaudioDevice::Stop()
    {
        if (!m_Impl || !m_Impl->bReady)
        {
            return;
        }

        // Uninit stops the device and waits for its thread.
        ma_device_uninit(&m_Impl->Device);
        m_Impl->bReady = false;
    }

    Uint32 MiniaudioDevice::GetPeriodFrames() const noexcept
    {
        return m_Impl->bReady ? static_cast<Uint32>(m_Impl->Device.playback.internalPeriodSizeInFrames) : 0;
    }
}
