#pragma once

#include "Core/OpaaxTypes.h"

#include <chrono>
#include <cstring>   // strcmp

namespace Opaax
{
    /**
     * One timed scope. Name must outlive the frame (pass a literal).
     */
    struct ScopeSample
    {
        const char* Name         = nullptr;

        /** Total time of every call this frame. */
        double      Milliseconds = 0.0;

        /** Times the scope was entered this frame. */
        Uint32      Calls        = 0;

        /** Nesting level, 0 at the top. Samples are in pre-order. */
        Uint8       Depth        = 0;
    };

    /**
     * One named counter for the frame (draw calls, quads, bullets alive, ...).
     */
    struct StatCounter
    {
        const char* Name  = nullptr;
        Uint64      Value = 0;
    };

    /**
     * Named timing scopes for one frame (like Unreal's SCOPE_CYCLE_COUNTER).
     * The engine's instance lives in the Profiler; tests can create their own.
     *
     * Double-buffered: Publish() makes the recorded frame readable and starts a new one,
     * so readers always see the last complete frame (including Present).
     */
    class FrameProfiler
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        FrameProfiler()
        {
            // Reserve both buffers (Publish swaps them), so a frame allocates nothing after warm-up.
            m_Recording.reserve(RESERVED_SAMPLES);
            m_Published.reserve(RESERVED_SAMPLES);
        }

        // =============================================================================
        // Record
        // =============================================================================
    public:
        /**
         * Begins a scope. A scope re-entered under the same parent reuses its row and counts a call.
         * @return The sample index, to pass to Close. Use ScopedStat instead of calling this directly.
         */
        Int32 Open(const char* InName)
        {
            Int32 lIndex = FindOpenSibling(InName);

            if (lIndex < 0)
            {
                lIndex = static_cast<Int32>(m_Recording.size());
                m_Recording.emplace_back(InName, 0.0, 0u, m_Depth);
            }

            ++m_Recording[lIndex].Calls;

            m_OpenStack.emplace_back(lIndex);
            ++m_Depth;

            return lIndex;
        }

        /**
         * Adds InValue to the counter named InName (created on first use).
         * InName must outlive the frame (pass a literal).
         */
        void AddCount(const char* InName, const Uint64 InValue)
        {
            for (StatCounter& lCounter : m_RecordingCounters)
            {
                if (SameName(lCounter.Name, InName))
                {
                    lCounter.Value += InValue;
                    return;
                }
            }

            m_RecordingCounters.emplace_back(InName, InValue);
        }

        /** Closes the scope and adds its duration to the total. */
        void Close(const Int32 InIndex, const double InMilliseconds)
        {
            if (!m_OpenStack.empty()) { m_OpenStack.pop_back(); }
            if (m_Depth > 0)          { --m_Depth; }

            if (InIndex < 0 || InIndex >= static_cast<Int32>(m_Recording.size())) { return; }

            m_Recording[InIndex].Milliseconds += InMilliseconds;
        }

        // =============================================================================
        // Frame boundary
        // =============================================================================
    public:
        /**
         * Ends the frame: the recorded scopes and counters become readable, recording starts empty.
         */
        void Publish()
        {
            m_Published.swap(m_Recording);
            m_Recording.clear();

            m_PublishedCounters.swap(m_RecordingCounters);
            m_RecordingCounters.clear();

            m_OpenStack.clear();
            m_Depth = 0;
        }

        // =============================================================================
        // Read
        // =============================================================================
    public:
        /** The last complete frame, in pre-order. Empty until the first Publish. */
        const TDynArray<ScopeSample>& Samples() const noexcept { return m_Published; }

        /** The last complete frame's counters, in order of first use. */
        const TDynArray<StatCounter>& Counters() const noexcept { return m_PublishedCounters; }

        bool IsEmpty() const noexcept { return m_Published.empty() && m_PublishedCounters.empty(); }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /**
         * Compares pointers first, then text (two literals with the same name).
         */
        static bool SameName(const char* InA, const char* InB) noexcept
        {
            if (InA == InB)                         { return true; }
            if (InA == nullptr || InB == nullptr)   { return false; }

            return std::strcmp(InA, InB) == 0;
        }

        /**
         * @return Index of the scope with this name under the open parent, or -1
         */
        Int32 FindOpenSibling(const char* InName) const
        {
            const Int32 lFirstChild = m_OpenStack.empty() ? 0 : m_OpenStack.back() + 1;

            for (Int32 i = lFirstChild; i < static_cast<Int32>(m_Recording.size()); ++i)
            {
                if (m_Recording[i].Depth != m_Depth) { continue; }

                if (SameName(m_Recording[i].Name, InName)) { return i; }
            }

            return -1;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static constexpr Uint32 RESERVED_SAMPLES = 64;

        TDynArray<ScopeSample> m_Recording;   // this frame, still filling
        TDynArray<ScopeSample> m_Published;   // last frame, complete
        TDynArray<StatCounter> m_RecordingCounters;
        TDynArray<StatCounter> m_PublishedCounters;
        TDynArray<Int32>       m_OpenStack;   // indices of the open scopes, outermost first
        Uint8                  m_Depth = 0;
    };

    /**
     * RAII scope around FrameProfiler::Open/Close. Does nothing on a null profiler.
     * Engine code uses OPAAX_STAT_SCOPE (Profiler.h).
     */
    class ScopedStat
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ScopedStat(FrameProfiler* InProfiler, const char* InName)
            : m_Profiler(InProfiler)
        {
            if (m_Profiler == nullptr) { return; }

            m_Index = m_Profiler->Open(InName);
            m_Start = std::chrono::steady_clock::now();
        }

        ~ScopedStat()
        {
            if (m_Profiler == nullptr) { return; }

            const std::chrono::duration<double, std::milli> lElapsed =
                std::chrono::steady_clock::now() - m_Start;

            m_Profiler->Close(m_Index, lElapsed.count());
        }

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        ScopedStat(const ScopedStat&)            = delete;
        ScopedStat& operator=(const ScopedStat&) = delete;
        ScopedStat(ScopedStat&&)                 = delete;
        ScopedStat& operator=(ScopedStat&&)      = delete;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        FrameProfiler*                        m_Profiler = nullptr;
        Int32                                 m_Index    = -1;
        std::chrono::steady_clock::time_point m_Start;
    };
}
