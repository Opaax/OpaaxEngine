#pragma once

#include "Core/OpaaxTypes.h"
#include "Renderer/Camera/CameraTrace.h"

namespace Opaax::Editor
{
    // =============================================================================
    // CameraStatsHistory — the last frames of the playing world, as a camera trace records them,
    //   turned into what the Camera Stats panel graphs: frame time, steps, alpha, and the target's
    //   drift on screen (CameraTrace::Drift, the measure camera.trace's limits check). No ImGui, so it
    //   is tested.
    // =============================================================================
    class CameraStatsHistory
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Four seconds at 60 frames a second. */
        static constexpr Uint64 CAPACITY = 240;

        /**
         * Adds a frame; the oldest goes once full.
         * @param InHeightPx The height of the view the drift is measured in, pixels
         */
        void Push(const CameraTraceSample& InSample, const float InHeightPx)
        {
            const float lDrift = m_bHasLast ? CameraTrace::Drift(m_Last, InSample, InHeightPx) : 0.f;

            Append(m_FrameMs, static_cast<float>(InSample.FrameDelta * 1000.0));
            Append(m_Steps, static_cast<float>(InSample.Steps));
            Append(m_Alpha, static_cast<float>(InSample.Alpha));
            Append(m_Drift, lDrift);

            m_Last     = InSample;
            m_bHasLast = true;
        }

        /** Starts over: another world, or another target. */
        void Clear() noexcept
        {
            m_FrameMs.clear();
            m_Steps.clear();
            m_Alpha.clear();
            m_Drift.clear();
            m_bHasLast = false;
        }

        /** The largest value of InSeries, 0 when empty. */
        static float Largest(const TDynArray<float>& InSeries) noexcept
        {
            float lLargest = 0.f;
            for (const float lValue : InSeries)
            {
                lLargest = lValue > lLargest ? lValue : lLargest;
            }
            return lLargest;
        }

    private:
        static void Append(TDynArray<float>& InOutSeries, const float InValue)
        {
            if (InOutSeries.size() >= CAPACITY)
            {
                InOutSeries.erase(InOutSeries.begin());
            }
            InOutSeries.push_back(InValue);
        }

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool                     HasFrames() const noexcept { return m_bHasLast; }
        const CameraTraceSample& GetLast() const noexcept   { return m_Last; }
        Uint64                   Count() const noexcept     { return m_FrameMs.size(); }

        /** Oldest first, one value a frame. */
        const TDynArray<float>& GetFrameMs() const noexcept { return m_FrameMs; }
        const TDynArray<float>& GetSteps() const noexcept   { return m_Steps; }
        const TDynArray<float>& GetAlpha() const noexcept   { return m_Alpha; }

        /** Pixels; 0 on a frame without a target, or across a camera switch or a cut. */
        const TDynArray<float>& GetDrift() const noexcept { return m_Drift; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<float>  m_FrameMs;
        TDynArray<float>  m_Steps;
        TDynArray<float>  m_Alpha;
        TDynArray<float>  m_Drift;
        CameraTraceSample m_Last;
        bool              m_bHasLast = false;
    };
}
