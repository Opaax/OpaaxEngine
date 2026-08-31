#pragma once

#include "Core/Profiling/FrameStats.h"

#include <cstring>   // strcmp — two literals may spell one scope name

namespace Opaax::Editor
{
    // =============================================================================
    // StatsDisplay — a FrameStats held STILL enough to read.
    //
    //   The engine's snapshot is per-frame and correct; drawing it directly is not readable. Two
    //   things move that should not:
    //
    //     - every number, 60 times a second. The caller fixes that by refreshing on a throttle.
    //     - the ROW SET. A frame whose accumulator took no fixed step has no FixedUpdate children,
    //       so the rows below jump up and back. That is what this type fixes: a scope already known
    //       keeps its place and reads 0.00 for the frames it did not run.
    //
    //   Rebuilds wholesale only when the frame is not a SUBSEQUENCE of what is displayed — a world
    //   change, or a module registering a new subsystem. Rare, and one jump then is honest.
    //
    //   Header-only and free of ImGui so OpaaxTests can reach it: the fold below has an ordering
    //   subtlety (the same name under two parents is two rows) that no smoke run would catch.
    // =============================================================================
    class StatsDisplay
    {
        // =============================================================================
        // Update
        // =============================================================================
    public:
        /** Take InStats, keeping the current layout where it still fits. */
        void Update(const FrameStats& InStats)
        {
            m_FrameMs = InStats.FrameMs;

            if (!TryFold(InStats.Profiler.Samples()))
            {
                m_Scopes = InStats.Profiler.Samples();
            }

            FoldCounters(InStats.Profiler.Counters());
        }

        /** Drop the layout — the scopes of a world that is gone would linger at 0.00 forever. */
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

        /** The frame's measured total — everything at depth 0. The remainder is what nobody timed. */
        double TopLevelMs() const noexcept
        {
            double lTotal = 0.0;
            for (const ScopeSample& lRow : m_Scopes)
            {
                // Depth 0 only: a nested scope's time is already inside its parent's, and charging
                // it twice would drive the unmeasured remainder negative.
                if (lRow.Depth == 0) { lTotal += lRow.Milliseconds; }
            }

            return lTotal;
        }

        /** Frame time nobody measured: the OS event poll, and the editor's own UI pass. */
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
         * Fold InIncoming into the displayed rows, or answer false if it does not fit.
         *
         * Matched as an ordered SUBSEQUENCE rather than by name: "Renderer" appears under both
         * Update and Render at the same depth, so a name lookup would post the render cost onto the
         * update row.
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

            // Zero FIRST, so a scope that stopped running reads 0.00 rather than holding a stale
            // number that looks like it is still costing something.
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
         * Counters keyed by NAME — no parent, no depth, so a plain lookup is enough and the
         * subsequence walk the scopes need would be overkill.
         *
         * A known counter that stops being submitted holds its place at 0 rather than collapsing,
         * for the reason the scopes do: a row vanishing moves every row under it.
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
