#pragma once

#include "Core/Maths/Maths.h"
#include "Core/Memory/Memory.h"

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // AudioRingBuffer — mixed frames waiting for the device: the audio thread writes them ahead,
    //   the device callback reads them. Lock-free, one writer and one reader; never allocates
    //   after construction.
    // =============================================================================
    class AudioRingBuffer
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        AudioRingBuffer(const Uint32 InCapacityFrames, const Uint32 InChannels)
            : m_Samples(static_cast<size_t>(InCapacityFrames) * InChannels, 0.f)
            , m_Capacity(InCapacityFrames)
            , m_Channels(InChannels)
        {
        }

        AudioRingBuffer(const AudioRingBuffer&)            = delete;
        AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Writer: frames that can be written now. */
        Uint32 GetFreeFrames() const noexcept
        {
            return m_Capacity - GetReadableFrames();
        }

        /** Frames written and not read yet. */
        Uint32 GetReadableFrames() const noexcept
        {
            return static_cast<Uint32>(m_Written.load(EMemoryOrder::Acquire) - m_Read.load(EMemoryOrder::Acquire));
        }

        /**
         * Writer only. Copies as many of InFrames as fit.
         * @return Frames written
         */
        Uint32 Write(const float* InFrames, const Uint32 InFrameCount)
        {
            const Uint64 lWritten = m_Written.load(EMemoryOrder::Relaxed);
            const Uint64 lRead    = m_Read.load(EMemoryOrder::Acquire);
            const Uint32 lCount   = Maths::Min(InFrameCount, m_Capacity - static_cast<Uint32>(lWritten - lRead));

            CopyIn(InFrames, lWritten, lCount);
            m_Written.store(lWritten + lCount, EMemoryOrder::Release);
            return lCount;
        }

        /**
         * Reader only. Copies up to InFrameCount frames out.
         * @return Frames read; fewer than asked when the writer is behind
         */
        Uint32 Read(float* OutFrames, const Uint32 InFrameCount)
        {
            const Uint64 lRead    = m_Read.load(EMemoryOrder::Relaxed);
            const Uint64 lWritten = m_Written.load(EMemoryOrder::Acquire);
            const Uint32 lCount   = Maths::Min(InFrameCount, static_cast<Uint32>(lWritten - lRead));

            CopyOut(lRead, lCount, OutFrames);
            m_Read.store(lRead + lCount, EMemoryOrder::Release);
            return lCount;
        }

        Uint32 GetCapacityFrames() const noexcept { return m_Capacity; }
        Uint32 GetChannels()       const noexcept { return m_Channels; }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** The ring's sample index of stream frame InPosition. */
        size_t SampleIndex(const Uint64 InPosition) const noexcept
        {
            return static_cast<size_t>(InPosition % m_Capacity) * m_Channels;
        }

        /** Frames from InPosition to the end of the ring, at most InCount: the part before the wrap. */
        Uint32 FramesBeforeWrap(const Uint64 InPosition, const Uint32 InCount) const noexcept
        {
            return Maths::Min(InCount, m_Capacity - static_cast<Uint32>(InPosition % m_Capacity));
        }

        void CopyIn(const float* InFrom, const Uint64 InPosition, const Uint32 InCount)
        {
            const Uint32 lFirst = FramesBeforeWrap(InPosition, InCount);
            Memory::Copy(m_Samples.data() + SampleIndex(InPosition), InFrom, sizeof(float) * lFirst * m_Channels);
            Memory::Copy(m_Samples.data(), InFrom + static_cast<size_t>(lFirst) * m_Channels,
                         sizeof(float) * (InCount - lFirst) * m_Channels);
        }

        void CopyOut(const Uint64 InPosition, const Uint32 InCount, float* OutTo) const
        {
            const Uint32 lFirst = FramesBeforeWrap(InPosition, InCount);
            Memory::Copy(OutTo, m_Samples.data() + SampleIndex(InPosition), sizeof(float) * lFirst * m_Channels);
            Memory::Copy(OutTo + static_cast<size_t>(lFirst) * m_Channels, m_Samples.data(),
                         sizeof(float) * (InCount - lFirst) * m_Channels);
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<float> m_Samples;
        Uint32           m_Capacity = 0;
        Uint32           m_Channels = 0;

        // Frames ever written and read: their difference is what waits.
        alignas(64) TAtomic<Uint64> m_Written{ 0 };
        alignas(64) TAtomic<Uint64> m_Read{ 0 };
    };
}
