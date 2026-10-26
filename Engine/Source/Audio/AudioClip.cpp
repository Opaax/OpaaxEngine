#include "Audio/AudioClip.h"

#include "Audio/AudioAPI.h"
#include "Audio/AudioTypes.h"
#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    namespace
    {
        /** Frames decoded per read when a whole clip is decoded. */
        constexpr Uint64 DECODE_CHUNK_FRAMES = 4096;
    }

    AudioClip::AudioClip()  = default;
    AudioClip::~AudioClip() = default;

    bool AudioClip::LoadSource(const char* InAbsPath)
    {
        TDynArray<Uint8> lBytes;
        if (!FileIO::ReadAllBytes(OpaaxString(InAbsPath), lBytes))
        {
            OPAAX_LOG(LogAudio, Error, "Audio clip '{}' cannot be read", InAbsPath);
            return false;
        }

        return LoadFromMemory(Move(lBytes), InAbsPath);
    }

    bool AudioClip::LoadFromMemory(TDynArray<Uint8> InBytes, const char* InNameForLogs, const EResidency InResidency)
    {
        if (InBytes.empty())
        {
            OPAAX_LOG(LogAudio, Error, "Audio clip '{}' is empty", InNameForLogs);
            return false;
        }

        TUniquePtr<IAudioDecoder> lDecoder = AudioAPI::OpenDecoder(InBytes.data(), InBytes.size());
        if (lDecoder == nullptr)
        {
            OPAAX_LOG(LogAudio, Error, "Audio clip '{}' is not a supported sound (.wav, .mp3, .flac)", InNameForLogs);
            return false;
        }

        TSharedPtr<AudioClipData> lData = MakeShared<AudioClipData>();
        lData->Format = lDecoder->GetFormat();

        const AudioFormat& lFormat       = lData->Format;
        const double       lSeconds      = lFormat.SampleRate > 0 ? static_cast<double>(lFormat.FrameCount) / lFormat.SampleRate : 0.0;
        const Uint64       lDecodedBytes = lFormat.FrameCount * lFormat.Channels * sizeof(float);

        // A length the format does not tell is a stream: it could be anything.
        bool bDecode = lFormat.FrameCount > 0 && lSeconds <= DECODE_MAX_SECONDS && lDecodedBytes <= DECODE_MAX_BYTES;
        if (InResidency != EResidency::Auto)
        {
            bDecode = InResidency == EResidency::Decoded;
        }

        if (bDecode)
        {
            // Read to the end: the length a format tells may be an estimate.
            lData->Frames.reserve(static_cast<size_t>(lFormat.FrameCount * lFormat.Channels));
            TDynArray<float> lChunk(static_cast<size_t>(DECODE_CHUNK_FRAMES * lFormat.Channels));

            Uint64 lRead = 0;
            while ((lRead = lDecoder->Read(lChunk.data(), DECODE_CHUNK_FRAMES)) > 0)
            {
                lData->Frames.insert(lData->Frames.end(), lChunk.begin(), lChunk.begin() + static_cast<PtrDiff>(lRead * lFormat.Channels));
            }

            lData->Format.FrameCount = lData->Frames.size() / lFormat.Channels;
            if (lData->Format.FrameCount == 0)
            {
                OPAAX_LOG(LogAudio, Error, "Audio clip '{}' has no samples", InNameForLogs);
                return false;
            }
        }
        else
        {
            lData->Encoded = Move(InBytes);
        }

        m_Channels   = lData->Format.Channels;
        m_SampleRate = lData->Format.SampleRate;
        m_FrameCount = lData->Format.FrameCount;
        m_Data       = Move(lData);
        return true;
    }

    Uint64 AudioClip::GetResourceSize() const
    {
        if (m_Data == nullptr) { return sizeof(AudioClip); }

        return sizeof(AudioClip) + (m_Data->IsStreamed() ? static_cast<Uint64>(m_Data->Encoded.size())
                                                         : static_cast<Uint64>(m_Data->Frames.size() * sizeof(float)));
    }
}
