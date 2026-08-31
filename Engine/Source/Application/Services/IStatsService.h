#pragma once

#include "IAppService.h"
#include "ILogger.h"
#include "Core/Profiling/FrameStats.h"

namespace Opaax
{
    inline constexpr LogCategory LogStats{"Stats"};

    // =============================================================================
    // IStatsService — the frame profiler, exposed as an app service (④).
    //
    //   AN APP SERVICE, not an engine subsystem, and I4's test says so: it knows nothing about
    //   textures or worlds and it does not tick — it is a PASSIVE FACILITY you submit scopes to,
    //   the Logger's shape exactly. What looks like a tick is BeginFrame, which is a submission:
    //   the HOST tells it a frame ended, from the same place it closes input's frame. IN2 settled
    //   that boundary — Engine::Loop is the engine's tick, not the frame — and putting the publish
    //   inside Loop was the same misplacement L28 describes.
    //
    //   THE PROFILER IS HANDED OUT AS A RAW POINTER, and that is what makes "stats off" free.
    //   Consumers cache it once in their Startup (F3), so there is no locator lookup and no virtual
    //   call per scope; a disabled build hands out nullptr and ScopedStat's existing null check
    //   costs one predicted branch. That is why there is no compile-time switch: an #if would buy
    //   back under a microsecond a frame and would make EnableInShipBuild unreachable.
    //
    //   Get<IStatsService>() NEVER returns null (I3). The null object profiles nothing, which is
    //   also exactly what a build with stats disabled gets — nothing is PROVIDED, so the locator's
    //   fallback IS the off switch. There is no second "disabled" state to keep correct.
    // =============================================================================
    class OPAAX_API IStatsService : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IStatsService)

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Close the frame that just ended and open the next one. Called by the HOST once per
         * iteration, beside InputManager::EndFrame (IN2) — the one place that owns the frame.
         *
         * Measures its own wall time between calls, so nothing has to hand it a delta; the first
         * call has no previous frame to report and yields 0.
         */
        virtual void BeginFrame() = 0;

        /**
         * Where OPAAX_STAT_SCOPE records, or NULL when stats are off.
         *
         * Resolve ONCE in Startup and cache it (F3). Null is a normal answer, not an error —
         * ScopedStat no-ops on it, so no caller needs a branch.
         */
        virtual FrameProfiler* GetProfiler() = 0;

        /** What the last COMPLETE frame cost. Empty and zeroed while stats are off. */
        virtual const FrameStats& GetFrameStats() const = 0;

        /** Whether anything is actually being measured — for a log line, not for a call-site branch. */
        virtual bool IsEnabled() const noexcept = 0;

        //----- null object ----------------------------------------------------
        static IStatsService& Null();
    };

    // =============================================================================
    // StatsService — the real one. Provided only when the build or the config asks for it, so its
    //   mere existence IS "stats are on" and nothing carries a disabled flag.
    // =============================================================================
    class OPAAX_API StatsService final : public IStatsService
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        StatsService();
        ~StatsService() override;

        StatsService(const StatsService&)            = delete;
        StatsService& operator=(const StatsService&) = delete;
        StatsService(StatsService&&)                 = delete;
        StatsService& operator=(StatsService&&)      = delete;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IStatsService interface
    public:
        void               BeginFrame() override;
        FrameProfiler*     GetProfiler() override            { return &m_Stats.Profiler; }
        const FrameStats&  GetFrameStats() const override    { return m_Stats; }
        bool               IsEnabled() const noexcept override { return true; }
        //~End IStatsService interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // NEVER swapped or moved — consumers cache &m_Stats.Profiler. Filled in place.
        FrameStats m_Stats;

        double m_LastFrameStart = 0.0;
        bool   m_bLoggedFirst   = false;
    };
}
