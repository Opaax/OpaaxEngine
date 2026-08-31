#pragma once

#include "Core/Profiling/StatsHistory.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Panels/StatsDisplay.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // StatsPanel — where the frame went (④). Reads IEngine::GetFrameStats() and nothing else, so
    //   the readout cannot disagree with what the engine measured.
    //
    //   Three blocks: the frame-time graph, the fixed-step count, and the scope tree — every named
    //   scope in the frame, indented by its depth. Anything a producer wraps in OPAAX_STAT_SCOPE
    //   appears here with no edit to this panel, which is the point of the profiler's shape.
    //
    //   EVERYTHING SLOW-MOVING IS DELIBERATE. The engine's snapshot is per-frame, and drawn raw it
    //   is a wall of digits changing 60 times a second. The graph is the only thing that moves per
    //   frame; the numbers refresh on a throttle and the row set is held still by StatsDisplay.
    //
    //   The HISTORY lives here, not in the engine: a snapshot is what every consumer agrees on, a
    //   history length is a display choice (F4 — the engine retains nothing).
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
        /** Frame time, fps, and the graph. The graph reads the history, so it moves every frame. */
        void DrawFrameTime();

        /** Every scope in the held snapshot, plus the unmeasured remainder. */
        void DrawBreakdown();

        /** The frame's named counters — draw calls, quads, and whatever a game submits. */
        void DrawCounters();

        /** Ceiling for the plot, in whole 60 Hz frames so it steps rather than drifting. */
        float GraphCeilingMs() const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — everything is read through the context. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        /** Samples the history, refreshes the held snapshot on the throttle, draws both blocks. */
        void DrawContents() override;

        /** Nothing to release. */
        void Shutdown()    override {}

        /** The old world's subsystems would otherwise sit in the tree at 0.00 forever. */
        void OnActiveWorldChanged(World* InOld, World* InNew) override;

        PanelWindowStyle GetWindowStyle() const override { return { { 420.f, 380.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // ~2 seconds at 60 Hz — long enough to see a hitch, short enough that the graph still
        // reacts to a change the author just made.
        static constexpr Uint32 HISTORY_SAMPLES = 120;

        // Four readable updates a second. The same interval EditorService throttles its dirty check
        // with, and far below what an eye reads as lag.
        static constexpr double REFRESH_INTERVAL = 0.25;

        // Never below this, so an idle 16 ms frame does not fill the plot and read as a problem.
        static constexpr float MIN_GRAPH_CEILING_MS = 33.3f;

        EditorContext& m_Context;

        // Pushed EVERY frame, so the graph is complete and smooth even though the numbers beside it
        // are not. Sampled in DrawContents, NOT in OnPreRender: OnPreRender runs before
        // Engine::Loop publishes, so it would push the frame BEFORE the one the text shows.
        TStatsHistory<HISTORY_SAMPLES> m_FrameHistory;

        StatsDisplay m_Display;
        double       m_LastRefresh = 0.0;

        // Held on the same throttle as m_Display, so NO text in this panel changes at frame rate.
        // The graph's ceiling deliberately still reads the live maximum: a spike arriving between
        // refreshes must not be clipped for a quarter of a second.
        float m_ShownAvgMs = 0.f;
        float m_ShownMinMs = 0.f;
        float m_ShownMaxMs = 0.f;
    };
}
