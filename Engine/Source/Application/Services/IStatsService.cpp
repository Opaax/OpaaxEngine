#include "IStatsService.h"

#include <chrono>

namespace Opaax
{
    namespace
    {
        double NowSeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }

        // =====================================================================
        // NullStatsService — what Get<IStatsService>() answers when nothing was provided, which is
        // ALSO the "stats disabled" state: the off switch is simply not providing the real one.
        // GetProfiler() is null, so every OPAAX_STAT_SCOPE in the tree becomes one predicted branch.
        // =====================================================================
        class NullStatsService final : public IStatsService
        {
        public:
            bool IsNull() const noexcept override { return true; }

            void               BeginFrame() override {}
            FrameProfiler*     GetProfiler() override { return nullptr; }
            bool               IsEnabled() const noexcept override { return false; }

            const FrameStats& GetFrameStats() const override
            {
                // Inert — no frame ever ran, so the total is zero and the tree is empty. That reads
                // correctly in a panel rather than needing a null branch there.
                static const FrameStats s_NullStats;
                return s_NullStats;
            }
        };
    }

    // =========================================================================
    // Type tag + null object (out-of-line — one instance across the DLL/exe line, I2).
    // =========================================================================
    ServiceTypeID IStatsService::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IStatsService& IStatsService::Null()
    {
        static NullStatsService s_Null;
        return s_Null;
    }

    // =========================================================================
    // StatsService
    // =========================================================================
    StatsService::StatsService()  = default;
    StatsService::~StatsService() = default;

    void StatsService::BeginFrame()
    {
        const double lNow = NowSeconds();

        // The frame that just ended becomes readable, whole — including its Present, which happens
        // after the engine's tick and would be missing from a list published inside Loop.
        m_Stats.Profiler.Publish();

        // First call has no previous frame; reporting the time since process start would put one
        // absurd sample at the front of every graph.
        m_Stats.FrameMs = m_LastFrameStart > 0.0 ? (lNow - m_LastFrameStart) * 1000.0 : 0.0;

        m_LastFrameStart = lNow;

        // ONE line, once — the success branch (L15). Nothing else in a boot log distinguishes
        // "measured the frame" from "recorded nothing at all".
        if (!m_bLoggedFirst && !m_Stats.Profiler.IsEmpty())
        {
            m_bLoggedFirst = true;

            OPAAX_LOG(LogStats, Info,
                      "Frame stats live — {} scope(s), {} counter(s) in the first measured frame ({:.2f} ms)",
                      m_Stats.Profiler.Samples().size(), m_Stats.Profiler.Counters().size(),
                      m_Stats.FrameMs);
        }
    }
}
