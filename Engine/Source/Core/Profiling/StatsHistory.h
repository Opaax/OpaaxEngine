#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * @class TStatsHistory
     *
     * A fixed ring of the last TCapacity samples, with the three numbers a readout wants (average,
     * min, max). Header-only value type, no OPAAX_API (I6).
     *
     * WHO RETAINS WHAT: the engine keeps no instance of this. FrameStats is one frame and nothing
     * more (F4), because a per-frame snapshot is what every consumer can agree on while a history
     * length is a DISPLAY choice — a graph wants 120 samples, a log line wants none. So the reader
     * owns its own history, and the type lives here beside FrameProfiler because that is where
     * someone would look for it, not because the engine holds one.
     *
     * @tparam TCapacity Samples kept. The oldest is dropped once it is full.
     */
    template<Uint32 TCapacity>
    requires (TCapacity > 0)
    class TStatsHistory
    {
        // =============================================================================
        // Record
        // =============================================================================
    public:
        void Push(const float InSample) noexcept
        {
            m_Samples[m_Next] = InSample;
            m_Next            = (m_Next + 1) % TCapacity;

            if (m_Count < TCapacity) { ++m_Count; }
        }

        void Clear() noexcept
        {
            m_Samples = {};
            m_Next    = 0;
            m_Count   = 0;
        }

        // =============================================================================
        // Read
        // =============================================================================
    public:
        /** The raw buffer — pair it with Offset(), which is where the OLDEST sample sits. */
        const float* Data() const noexcept { return m_Samples.data(); }

        /** How many samples are valid. Below Capacity() until the ring has filled once. */
        Uint32 Count() const noexcept { return m_Count; }

        static constexpr Uint32 Capacity() noexcept { return TCapacity; }

        /**
         * Index of the oldest valid sample, so a plot reads Data() from here and wraps.
         *
         * Zero until the ring is full: before that the samples are simply [0, Count) in order, and
         * m_Next is one past the newest rather than the oldest.
         */
        Uint32 Offset() const noexcept { return m_Count == TCapacity ? m_Next : 0; }

        /** Zero when empty — an honest answer for a readout, and no special case at the call site. */
        float Average() const noexcept
        {
            if (m_Count == 0) { return 0.f; }

            float lSum = 0.f;
            for (Uint32 i = 0; i < m_Count; ++i) { lSum += At(i); }

            return lSum / static_cast<float>(m_Count);
        }

        float Min() const noexcept
        {
            if (m_Count == 0) { return 0.f; }

            float lMin = At(0);
            for (Uint32 i = 1; i < m_Count; ++i) { lMin = At(i) < lMin ? At(i) : lMin; }

            return lMin;
        }

        float Max() const noexcept
        {
            if (m_Count == 0) { return 0.f; }

            float lMax = At(0);
            for (Uint32 i = 1; i < m_Count; ++i) { lMax = At(i) > lMax ? At(i) : lMax; }

            return lMax;
        }

        /** Sample InIndex counting from the OLDEST. Out of range answers 0. */
        float At(const Uint32 InIndex) const noexcept
        {
            if (InIndex >= m_Count) { return 0.f; }

            return m_Samples[(Offset() + InIndex) % TCapacity];
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TFixedArray<float, TCapacity> m_Samples{};
        Uint32                        m_Next  = 0;   // where the NEXT push lands
        Uint32                        m_Count = 0;
    };
}
