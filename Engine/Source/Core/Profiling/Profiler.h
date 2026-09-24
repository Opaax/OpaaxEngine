#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMacro.hpp"   // OPAAX_CONCAT — the macro's unique local name
#include "Core/Log/Logger.h"
#include "Core/Profiling/FrameStats.h"

#include <thread>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Stats);

    // =============================================================================
    // Profiler — an I1 singleton (SG1–SG5): the one FrameStats of the process. The host says where a
    //   frame ends (BeginFrame, ST2); anyone may open a scope or submit a counter.
    //
    //   Disabled is the off switch (ST6): every scope then costs one predicted branch and no clock
    //   read. Recording is MAIN-THREAD ONLY — FrameProfiler is not thread-safe, and with a global a
    //   worker could otherwise reach it; off-thread scopes and counters are no-ops.
    // =============================================================================
    class OPAAX_API Profiler final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        /** Out-of-line in the DLL and leaked (SG4/SG5). */
        static Profiler& Get();

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        Profiler();
        ~Profiler();

        Profiler(const Profiler&)            = delete;
        Profiler& operator=(const Profiler&) = delete;
        Profiler(Profiler&&)                 = delete;
        Profiler& operator=(Profiler&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** The CALLING thread becomes the one that records. */
        void Init(bool bInEnabled);

        /** Disables recording; the last published frame stays readable. */
        void Shutdown();

        /** Publishes the frame that just ended and opens the next (ST2). */
        void BeginFrame();

        /** The device's reading, folded into the next published frame (ST8). */
        void SubmitGpuMs(double InGpuMs);

        /** A named counter for this frame (ST7). Accumulates. */
        void AddCount(const char* InName, Uint64 InValue);

        // =============================================================================
        // Getters
        // =============================================================================
    public:
        /** Null when disabled or off the recording thread — what OPAAX_STAT_SCOPE opens on. */
        FrameProfiler* GetRecorder() noexcept
        {
            return m_bEnabled && std::this_thread::get_id() == m_RecordingThread ? &m_Stats.Profiler : nullptr;
        }

        /** The last COMPLETE frame. Empty and zeroed while disabled. */
        const FrameStats& GetFrameStats() const noexcept { return m_Stats; }

        bool IsEnabled() const noexcept { return m_bEnabled; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Filled in place, never swapped: a reader may hold a reference across frames.
        FrameStats      m_Stats;
        std::thread::id m_RecordingThread;

        double m_RecordingGpuMs = -1.0;
        double m_LastFrameStart = 0.0;

        bool m_bEnabled        = false;
        bool m_bLoggedFirst    = false;
        bool m_bLoggedFirstGpu = false;
    };
}

/**
 * Time the enclosing block, Unreal's SCOPE_CYCLE_COUNTER shape. OPT-IN (ST3); pass a literal, the
 * name must outlive the frame.
 *
 *   OPAAX_STAT_SCOPE("Sprites");
 */
#define OPAAX_STAT_SCOPE(InName) \
    const ::Opaax::ScopedStat OPAAX_CONCAT(lStatScope_, __LINE__)(::Opaax::Profiler::Get().GetRecorder(), (InName))
