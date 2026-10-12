#include "Audio/Mixer/AudioStreamReader.h"

#include <algorithm>
#include <cstring>

namespace Opaax
{
    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    AudioStreamReader::AudioStreamReader(TUniquePtr<IAudioDecoder> InDecoder, const Uint32 InWindowFrames)
        : m_Decoder(Move(InDecoder))
    {
        if (m_Decoder != nullptr)
        {
            m_Channels   = m_Decoder->GetFormat().Channels;
            m_SampleRate = m_Decoder->GetFormat().SampleRate;
        }

        m_WindowCapacity = std::max(InWindowFrames, 1u);
        m_Window.resize(static_cast<size_t>(m_WindowCapacity) * std::max(m_Channels, 1u), 0.f);
        m_bEnded = m_Decoder == nullptr || m_Channels == 0;
    }

    // =========================================================================
    // Functions
    // =========================================================================
    Uint32 AudioStreamReader::Fetch(const Uint64 InFirst, const Uint32 InCount, const bool bInLoop, float* OutFrames)
    {
        const size_t lChannels = std::max(m_Channels, 1u);

        // Forget the frames before InFirst: the window slides forward.
        if (InFirst > m_WindowStart)
        {
            const Uint32 lDrop = static_cast<Uint32>(std::min<Uint64>(InFirst - m_WindowStart, m_WindowCount));
            std::memmove(m_Window.data(), m_Window.data() + lDrop * lChannels, (m_WindowCount - lDrop) * lChannels * sizeof(float));
            m_WindowCount -= lDrop;
            m_WindowStart += lDrop;

            // Further than what was decoded: decode and skip up to InFirst.
            while (m_WindowStart < InFirst && !m_bEnded)
            {
                FillTo(std::min<Uint64>(InFirst, m_WindowStart + m_WindowCapacity), bInLoop);
                const Uint32 lSkip = static_cast<Uint32>(std::min<Uint64>(InFirst - m_WindowStart, m_WindowCount));
                std::memmove(m_Window.data(), m_Window.data() + lSkip * lChannels, (m_WindowCount - lSkip) * lChannels * sizeof(float));
                m_WindowCount -= lSkip;
                m_WindowStart += lSkip;

                if (lSkip == 0)
                {
                    break;
                }
            }
        }

        const Uint32 lCount = std::min(InCount, m_WindowCapacity);
        FillTo(InFirst + lCount, bInLoop);

        // What the window holds from InFirst on; the rest is silence.
        const Uint64 lWindowEnd = m_WindowStart + m_WindowCount;
        const Uint32 lValid     = InFirst >= lWindowEnd ? 0u : static_cast<Uint32>(std::min<Uint64>(lWindowEnd - InFirst, lCount));
        const size_t lOffset    = static_cast<size_t>(InFirst - std::min(InFirst, m_WindowStart)) * lChannels;

        std::memcpy(OutFrames, m_Window.data() + lOffset, lValid * lChannels * sizeof(float));
        std::memset(OutFrames + lValid * lChannels, 0, (InCount - lValid) * lChannels * sizeof(float));
        return lValid;
    }

    // =========================================================================
    // Internal
    // =========================================================================
    void AudioStreamReader::FillTo(const Uint64 InEnd, const bool bInLoop)
    {
        const size_t lChannels = std::max(m_Channels, 1u);
        bool bRewound = false;

        while (!m_bEnded && m_WindowStart + m_WindowCount < InEnd && m_WindowCount < m_WindowCapacity)
        {
            const Uint64 lWanted = std::min<Uint64>(InEnd - (m_WindowStart + m_WindowCount), m_WindowCapacity - m_WindowCount);
            const Uint64 lRead   = m_Decoder->Read(m_Window.data() + m_WindowCount * lChannels, lWanted);
            m_WindowCount += static_cast<Uint32>(lRead);

            if (lRead > 0)
            {
                bRewound = false;
                continue;
            }

            // The end of the sound. Rewinding twice in a row with nothing read: an empty sound.
            if (bInLoop && !bRewound && m_Decoder->Rewind())
            {
                bRewound = true;
                continue;
            }

            m_bEnded = true;
        }
    }
}
