#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMacro.hpp"   // OPAAX_CONCAT — the macro's unique local name

#include <chrono>
#include <cstring>   // strcmp — the fallback when two literals spell the same name

namespace Opaax
{
    /**
     * One timed scope. Plain data — no lifetime, no ownership.
     *
     * Name is BORROWED and must outlive the frame; every producer passes a literal (the
     * OPAAX_STAT_SCOPE call site passes a literal). Not an
     * OpaaxStringView — I13 gives the view no CStr() on purpose and a UI needs a terminator; not an
     * OpaaxStringID — interning is for keys, and this is display text on a per-frame path.
     */
    struct ScopeSample
    {
        const char* Name         = nullptr;

        /** TOTAL across every call this frame, not one call's cost. */
        double      Milliseconds = 0.0;

        /** How many times the scope was entered this frame. FixedUpdate's children run once per step. */
        Uint32      Calls        = 0;

        /** Nesting level, 0 for a top-level scope. Samples are in pre-order, so this alone draws the tree. */
        Uint8       Depth        = 0;
    };

    /**
     * @class FrameProfiler
     *
     * The frame's named scopes, in the shape every engine has one (Unreal's SCOPE_CYCLE_COUNTER,
     * Unity's ProfilerMarker) minus the part this codebase forbids: those reach a GLOBAL stat
     * manager, and I1 allows no second static. So a scope takes its profiler by pointer, and the two
     * tiers that would otherwise have nowhere to get one already have a carrier —
     * IEngine::GetProfiler() for an engine subsystem, WorldContext::Profiler for a world subsystem.
     *
     * Core knows nothing about any of that: this file is a timer and a list. Nothing ticks it,
     * nothing is wrapped on an author's behalf, and ISubsystem does not mention it.
     *
     * DOUBLE-BUFFERED, and that is load-bearing rather than tidy. The wall-clock frame is
     * Loop(N){Update,FixedUpdate,Render} -> editor UI pass(N) -> Present(N) -> Loop(N+1), and the
     * Stats panel draws inside the UI pass. A single live list would hand it a frame with Present
     * missing — the one row where the time actually is under vsync. Publishing at Loop's top means a
     * reader always sees the last COMPLETE frame, one frame old.
     *
     * Nothing survives Publish(), matching DebugDraw's immediate-mode contract (F4): a scope that
     * wants to be seen is re-entered every frame.
     *
     * Header-only with no static and no identity tag, so NO OPAAX_API (I6's header-only shape, the
     * one ISubsystemManager already uses).
     */
    class FrameProfiler
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        FrameProfiler()
        {
            // Both buffers, because Publish swaps them — reserving one would still allocate on the
            // second frame. After warm-up a frame allocates nothing.
            m_Recording.reserve(RESERVED_SAMPLES);
            m_Published.reserve(RESERVED_SAMPLES);
        }

        // =============================================================================
        // Record
        // =============================================================================
    public:
        /**
         * Begin a scope. The sample is recorded HERE, not on Close — reserving the slot on the way
         * in is what leaves Samples() in pre-order, so Depth alone renders the tree with no sort.
         *
         * A scope re-entered under the SAME parent reuses its row and counts a call, so the tree has
         * one row per name however many times it ran (Unreal's stat rows, and the reason the display
         * survives a catch-up frame: FixedUpdate ticks every subsystem once per step, so 15 steps
         * would otherwise be 15 identical rows each).
         *
         * @return The sample's index, to hand back to Close. Callers use ScopedStat instead.
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

        /** Close the scope Open returned InIndex for, ADDING how long this call took to its total. */
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
        /** The frame is over: what was recorded becomes readable, and recording starts empty. */
        void Publish()
        {
            m_Published.swap(m_Recording);   // swap, not copy — capacity stays with both buffers
            m_Recording.clear();
            m_OpenStack.clear();
            m_Depth = 0;
        }

        // =============================================================================
        // Read
        // =============================================================================
    public:
        /** The last COMPLETE frame, in pre-order. Empty until the first Publish. */
        const TDynArray<ScopeSample>& Samples() const noexcept { return m_Published; }

        bool IsEmpty() const noexcept { return m_Published.empty(); }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /**
         * An already-recorded scope with this name under the CURRENTLY OPEN parent, or -1.
         *
         * Everything after the open parent is inside its subtree — nothing shallower can have been
         * appended while it is still open — so the depth test alone separates children from
         * grandchildren.
         */
        Int32 FindOpenSibling(const char* InName) const
        {
            const Int32 lFirstChild = m_OpenStack.empty() ? 0 : m_OpenStack.back() + 1;

            for (Int32 i = lFirstChild; i < static_cast<Int32>(m_Recording.size()); ++i)
            {
                if (m_Recording[i].Depth != m_Depth) { continue; }

                // Pointer first — one call site means one literal, so this hits every time in
                // practice. strcmp only covers two literals that happen to spell the same name,
                // which would otherwise show up as two identical rows.
                if (m_Recording[i].Name == InName) { return i; }

                if (m_Recording[i].Name != nullptr && InName != nullptr
                    && std::strcmp(m_Recording[i].Name, InName) == 0)
                {
                    return i;
                }
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
        TDynArray<Int32>       m_OpenStack;   // indices of the scopes currently open, outermost first
        Uint8                  m_Depth = 0;
    };

    /**
     * @class ScopedStat
     *
     * RAII around FrameProfiler::Open/Close. Takes a POINTER and no-ops on null, so a manager with
     * no profiler attached branches nowhere.
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
        // Copy - Move Delete (a scope is tied to one lexical block)
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

/**
 * Time the enclosing block, Unreal's SCOPE_CYCLE_COUNTER shape.
 *
 *   OPAAX_STAT_SCOPE(m_Profiler, "Sprites");
 *
 * OPT-IN, always: nothing is measured unless an author asks for it here. Blanket-wrapping every
 * subsystem tick was tried and produced rows for work that does not exist (an InputManager under
 * Render), which is noise a reader has to learn to ignore.
 *
 * InProfiler MAY BE NULL, and that is the whole off switch. A build with stats disabled provides no
 * stats service, so IStatsService::Null() hands out nullptr and this costs one predicted branch —
 * no clock read, no virtual call. There is deliberately NO compile-time flag: it would buy back
 * under a microsecond a frame and would make Stats.EnableInShipBuild unreachable, since you cannot
 * runtime-enable what was compiled out.
 *
 * The name must outlive the frame — pass a literal.
 */
#define OPAAX_STAT_SCOPE(InProfiler, InName) \
    const ::Opaax::ScopedStat OPAAX_CONCAT(lStatScope_, __LINE__)((InProfiler), (InName))
