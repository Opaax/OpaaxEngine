#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMacro.hpp"
#include "Core/Log/Logger.h"
#include "Core/Profiling/FrameStats.h"

#include <thread>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Stats);

    // =============================================================================
    // Profiler — the engine-wide frame stats. The host marks the frame boundary (BeginFrame);
    //   anyone can open a scope or submit a counter. When disabled, a scope costs one branch.
    //   Main thread only: scopes and counters from other threads are ignored.
    // =============================================================================
    class OPAAX_API Profiler final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        /** Never destroyed. */
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
        /** The calling thread becomes the recording thread. */
        void Init(bool bInEnabled);

        /** Disables recording; the last published frame stays readable. */
        void Shutdown();

        /** Publishes the frame that just ended and starts the next. */
        void BeginFrame();

        /** GPU time, added to the next published frame. */
        void SubmitGpuMs(double InGpuMs);

        /** Adds to a named counter for this frame. */
        void AddCount(const char* InName, Uint64 InValue);

        // =============================================================================
        // Getters
        // =============================================================================
    public:
        /** Null when disabled or not on the recording thread. */
        FrameProfiler* GetRecorder() noexcept
        {
            return m_bEnabled && std::this_thread::get_id() == m_RecordingThread ? &m_Stats.Profiler : nullptr;
        }

        /** The last complete frame. Empty when disabled. */
        const FrameStats& GetFrameStats() const noexcept { return m_Stats; }

        bool IsEnabled() const noexcept { return m_bEnabled; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Updated in place: a reader may keep a reference across frames.
        FrameStats      m_Stats;
        std::thread::id m_RecordingThread;

        double m_RecordingGpuMs = -1.0;
        double m_LastFrameStart = 0.0;

        bool m_bEnabled = false;
    };
}

/**
 * Times the enclosing block. The name must be a literal.
 *
 *   OPAAX_STAT_SCOPE("Sprites");
 */
#define OPAAX_STAT_SCOPE(InName) \
    const ::Opaax::ScopedStat OPAAX_CONCAT(lStatScope_, __LINE__)(::Opaax::Profiler::Get().GetRecorder(), (InName))
