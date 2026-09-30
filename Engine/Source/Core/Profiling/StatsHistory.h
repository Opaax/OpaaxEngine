#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * Ring buffer of the last TCapacity samples, with average, min and max.
     * @tparam TCapacity Samples kept; the oldest is dropped when full.
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
        /** The raw buffer. The oldest sample is at Offset(). */
        const float* Data() const noexcept { return m_Samples.data(); }

        /** Number of valid samples. */
        Uint32 Count() const noexcept { return m_Count; }

        static constexpr Uint32 Capacity() noexcept { return TCapacity; }

        /**
         * Index of the oldest sample (0 until the ring is full).
         */
        Uint32 Offset() const noexcept { return m_Count == TCapacity ? m_Next : 0; }

        /** 0 when empty. */
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

        /** Sample InIndex, counting from the oldest. 0 when out of range. */
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
        Uint32                        m_Next  = 0;   // where the next push goes
        Uint32                        m_Count = 0;
    };
}
