#include "Audio/Miniaudio/MiniaudioDecoder.h"

#include <miniaudio.h>

namespace Opaax
{
    namespace
    {
        /** The mixer reads mono or stereo: a sound with more channels is mixed down to stereo. */
        constexpr ma_uint32 MAX_DECODED_CHANNELS = 2;
    }

    struct MiniaudioDecoder::Impl
    {
        ma_decoder Decoder{};
        bool       bReady = false;
    };

    MiniaudioDecoder::MiniaudioDecoder()
        : m_Impl(MakeUnique<Impl>())
    {
    }

    MiniaudioDecoder::~MiniaudioDecoder()
    {
        if (m_Impl->bReady)
        {
            ma_decoder_uninit(&m_Impl->Decoder);
        }
    }

    TUniquePtr<IAudioDecoder> MiniaudioDecoder::Open(const Uint8* InBytes, const Uint64 InSize)
    {
        if (InBytes == nullptr || InSize == 0)
        {
            return nullptr;
        }

        TUniquePtr<MiniaudioDecoder> lDecoder(new MiniaudioDecoder());
        ma_decoder& lMa = lDecoder->m_Impl->Decoder;

        // The file's own channels and rate first; more than stereo is opened again, mixed down.
        ma_decoder_config lConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decoder_init_memory(InBytes, static_cast<size_t>(InSize), &lConfig, &lMa) != MA_SUCCESS)
        {
            return nullptr;
        }

        if (lMa.outputChannels > MAX_DECODED_CHANNELS)
        {
            ma_decoder_uninit(&lMa);
            lConfig = ma_decoder_config_init(ma_format_f32, MAX_DECODED_CHANNELS, 0);
            if (ma_decoder_init_memory(InBytes, static_cast<size_t>(InSize), &lConfig, &lMa) != MA_SUCCESS)
            {
                return nullptr;
            }
        }
        lDecoder->m_Impl->bReady = true;

        ma_uint64 lFrames = 0;
        const bool bKnownLength = ma_decoder_get_length_in_pcm_frames(&lMa, &lFrames) == MA_SUCCESS;

        lDecoder->m_Format.Channels   = lMa.outputChannels;
        lDecoder->m_Format.SampleRate = lMa.outputSampleRate;
        lDecoder->m_Format.FrameCount = bKnownLength ? static_cast<Uint64>(lFrames) : 0;

        if (lDecoder->m_Format.Channels == 0 || lDecoder->m_Format.SampleRate == 0)
        {
            return nullptr;
        }
        return lDecoder;
    }

    Uint64 MiniaudioDecoder::Read(float* OutFrames, const Uint64 InFrameCount)
    {
        ma_uint64 lRead = 0;
        ma_decoder_read_pcm_frames(&m_Impl->Decoder, OutFrames, InFrameCount, &lRead);
        return static_cast<Uint64>(lRead);
    }

    bool MiniaudioDecoder::Rewind()
    {
        return ma_decoder_seek_to_pcm_frame(&m_Impl->Decoder, 0) == MA_SUCCESS;
    }
}
