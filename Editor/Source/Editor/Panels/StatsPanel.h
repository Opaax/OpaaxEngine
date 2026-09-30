#pragma once

#include "Core/Profiling/StatsHistory.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Panels/StatsDisplay.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // StatsPanel — where the frame time went. Reads Profiler::GetFrameStats() only.
    //   The frame-time graph, the counters, and every named scope indented by depth (anything wrapped
    //   in OPAAX_STAT_SCOPE shows up). Only the graph moves every frame; the numbers refresh on a
    //   throttle and StatsDisplay keeps the rows still. The history lives here, not in the engine.
    // =============================================================================
    class StatsPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Stats);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit StatsPanel(EditorContext& InContext);
        ~StatsPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        StatsPanel(const StatsPanel&)            = delete;
        StatsPanel& operator=(const StatsPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Frame time, fps, and the graph (updated every frame). */
        void DrawFrameTime();

        /** Every scope in the held snapshot, plus the unmeasured remainder. */
        void DrawBreakdown();

        /** The frame's counters (draw calls, quads, and whatever a game submits). */
        void DrawCounters();

        /** The plot's ceiling, in whole 60 Hz frames. */
        float GraphCeilingMs() const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        /** Samples the history, refreshes the snapshot on the throttle, draws. */
        void DrawContents() override;

        /** Nothing to release. */
        void Shutdown()    override {}

        /** Clears the rows (the old world's subsystems would stay at 0.00 forever). */
        void OnActiveWorldChanged(World* InOld, World* InNew) override;

        PanelWindowStyle GetWindowStyle() const override { return { { 420.f, 380.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // ~2 seconds at 60 Hz.
        static constexpr Uint32 HISTORY_SAMPLES = 120;

        // Four updates per second.
        static constexpr double REFRESH_INTERVAL = 0.25;

        // Minimum ceiling, so an idle 16 ms frame does not fill the plot.
        static constexpr float MIN_GRAPH_CEILING_MS = 33.3f;

        EditorContext& m_Context;

        // Pushed every frame. Sampled in DrawContents: OnPreRender runs before Engine::Loop publishes.
        TStatsHistory<HISTORY_SAMPLES> m_FrameHistory;

        StatsDisplay m_Display;
        double       m_LastRefresh = 0.0;

        // Held on the same throttle, so no text changes every frame. The graph's ceiling still reads the
        // live maximum (a spike must not be clipped).
        float m_ShownAvgMs = 0.f;
        float m_ShownMinMs = 0.f;
        float m_ShownMaxMs = 0.f;

        // Negative = no reading.
        double m_ShownGpuMs = -1.0;
    };
}
