#include "Audio/AudioClipResource.h"

#include <miniaudio.h>

#include "Audio/AudioTypes.h"
#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    std::optional<AudioClipResource> AudioClipResource::Load(const char* InPath, LoadContext& /*InCtx*/)
    {
        TDynArray<Uint8> lBytes;
        if (!FileIO::ReadAllBytes(OpaaxString(InPath), lBytes))
        {
            OPAAX_LOG(LogAudio, Error, "Audio clip '{}' cannot be read", InPath);
            return std::nullopt;
        }

        return FromMemory(Move(lBytes), InPath);
    }

    std::optional<AudioClipResource> AudioClipResource::FromMemory(TDynArray<Uint8> InBytes, const char* InNameForLogs)
    {
        if (InBytes.empty())
        {
            OPAAX_LOG(LogAudio, Error, "Audio clip '{}' is empty", InNameForLogs);
            return std::nullopt;
        }

        // The file's own format: only the header is read here; sounds decode while they play.
        const ma_decoder_config lConfig = ma_decoder_config_init(ma_format_f32, 0, 0);

        ma_decoder lDecoder;
        if (ma_decoder_init_memory(InBytes.data(), InBytes.size(), &lConfig, &lDecoder) != MA_SUCCESS)
        {
            OPAAX_LOG(LogAudio, Error, "Audio clip '{}' is not a supported sound (.wav, .mp3, .flac)", InNameForLogs);
            return std::nullopt;
        }

        ma_uint64 lFrames = 0;
        const bool bKnownLength = ma_decoder_get_length_in_pcm_frames(&lDecoder, &lFrames) == MA_SUCCESS;

        AudioClipResource lClip;
        lClip.Channels   = lDecoder.outputChannels;
        lClip.SampleRate = lDecoder.outputSampleRate;
        lClip.FrameCount = bKnownLength ? static_cast<Uint64>(lFrames) : 0;

        ma_decoder_uninit(&lDecoder);

        if (lClip.Channels == 0 || lClip.SampleRate == 0)
        {
            OPAAX_LOG(LogAudio, Error, "Audio clip '{}' has no channel or no sample rate", InNameForLogs);
            return std::nullopt;
        }

        lClip.Encoded = MakeShared<const TDynArray<Uint8>>(Move(InBytes));
        return lClip;
    }
}
