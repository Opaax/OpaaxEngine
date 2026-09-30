#pragma once

#include "Core/Profiling/FrameStats.h"

#include <cstring>   // strcmp

namespace Opaax::Editor
{
    // =============================================================================
    // StatsDisplay — FrameStats held still enough to read. The caller refreshes it on a throttle;
    //   this keeps the row set stable: a known scope keeps its row and shows 0.00 on frames it did
    //   not run. Rebuilt only when the frame is not a subsequence of what is shown (world change,
    //   new subsystem). No ImGui, so it is tested.
    // =============================================================================
    class StatsDisplay
    {
        // =============================================================================
        // Update
        // =============================================================================
    public:
        /** Takes InStats, keeping the current layout where it fits. */
        void Update(const FrameStats& InStats)
        {
            m_FrameMs = InStats.FrameMs;

            if (!TryFold(InStats.Profiler.Samples()))
            {
                m_Scopes = InStats.Profiler.Samples();
            }

            FoldCounters(InStats.Profiler.Counters());
        }

        /** Drops the layout (a destroyed world's scopes would stay at 0.00 forever). */
        void Clear() noexcept
        {
            m_Scopes.clear();
            m_Counters.clear();
            m_FrameMs = 0.0;
        }

        // =============================================================================
        // Read
        // =============================================================================
    public:
        double FrameMs() const noexcept { return m_FrameMs; }

        const TDynArray<ScopeSample>& Scopes() const noexcept { return m_Scopes; }

        const TDynArray<StatCounter>& Counters() const noexcept { return m_Counters; }

        /** The frame's measured total (depth 0). The remainder is what nobody timed. */
        double TopLevelMs() const noexcept
        {
            double lTotal = 0.0;
            for (const ScopeSample& lRow : m_Scopes)
            {
                // Depth 0 only: a nested scope's time is already in its parent's.
                if (lRow.Depth == 0) { lTotal += lRow.Milliseconds; }
            }

            return lTotal;
        }

        /** Frame time nobody measured: the OS event poll and the editor's UI pass. */
        double UnmeasuredMs() const noexcept
        {
            const double lRest = m_FrameMs - TopLevelMs();
            return lRest > 0.0 ? lRest : 0.0;
        }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        static bool SameScope(const ScopeSample& InA, const ScopeSample& InB) noexcept
        {
            if (InA.Depth != InB.Depth)         { return false; }
            if (InA.Name == InB.Name)           { return true; }
            if (InA.Name == nullptr || InB.Name == nullptr) { return false; }

            return std::strcmp(InA.Name, InB.Name) == 0;
        }

        /**
         * Folds InIncoming into the shown rows, or returns false if it does not fit. Matched as an ordered
         * subsequence, not by name ("Renderer" appears under both Update and Render).
         */
        bool TryFold(const TDynArray<ScopeSample>& InIncoming)
        {
            TDynArray<Uint64> lMap;
            lMap.reserve(InIncoming.size());

            Uint64 lCursor = 0;
            for (const ScopeSample& lIn : InIncoming)
            {
                while (lCursor < m_Scopes.size() && !SameScope(m_Scopes[lCursor], lIn)) { ++lCursor; }

                if (lCursor >= m_Scopes.size()) { return false; }

                lMap.emplace_back(lCursor);
                ++lCursor;
            }

            // Zero first, so a scope that stopped running shows 0.00, not a stale number.
            for (ScopeSample& lRow : m_Scopes)
            {
                lRow.Milliseconds = 0.0;
                lRow.Calls        = 0;
            }

            for (Uint64 i = 0; i < InIncoming.size(); ++i)
            {
                m_Scopes[lMap[i]].Milliseconds = InIncoming[i].Milliseconds;
                m_Scopes[lMap[i]].Calls        = InIncoming[i].Calls;
            }

            return true;
        }

        /**
         * Counters, by name. A counter no longer submitted keeps its row at 0 (so rows do not jump).
         */
        void FoldCounters(const TDynArray<StatCounter>& InIncoming)
        {
            for (StatCounter& lRow : m_Counters) { lRow.Value = 0; }

            for (const StatCounter& lIn : InIncoming)
            {
                StatCounter* lRow = nullptr;
                for (StatCounter& lCandidate : m_Counters)
                {
                    if (SameName(lCandidate.Name, lIn.Name)) { lRow = &lCandidate; break; }
                }

                if (lRow != nullptr) { lRow->Value = lIn.Value; }
                else                 { m_Counters.emplace_back(lIn); }
            }
        }

        static bool SameName(const char* InA, const char* InB) noexcept
        {
            if (InA == InB)                       { return true; }
            if (InA == nullptr || InB == nullptr) { return false; }

            return std::strcmp(InA, InB) == 0;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<ScopeSample> m_Scopes;
        TDynArray<StatCounter> m_Counters;
        double                 m_FrameMs = 0.0;
    };
}
