#include "Editor/Panels/StatsPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/UI/IEditorGui.h"   // GetTime — the throttle's clock

#include "Application/Services/IStatsService.h"

#include <imgui.h>

#include <cmath>
#include <cstring>

using namespace Opaax;

namespace
{
    /** Pixels of indent per nesting level in the scope tree. */
    constexpr float k_ScopeIndent = 14.f;

    /** One 60 Hz frame. The graph's ceiling steps in these so it never drifts under the line. */
    constexpr float k_FrameMs60 = 1000.f / 60.f;

    /** Whether a counter carries this exact name. */
    bool SameCounter(const Opaax::StatCounter& InCounter, const char* InName)
    {
        return InCounter.Name != nullptr && std::strcmp(InCounter.Name, InName) == 0;
    }

    /** Percentage of the frame InMilliseconds represents. Zero for a frame with no duration. */
    float PercentOfFrame(const double InMilliseconds, const double InFrameMs)
    {
        if (InFrameMs <= 0.0) { return 0.f; }

        return static_cast<float>(InMilliseconds / InFrameMs * 100.0);
    }
}

namespace Opaax::Editor
{
    StatsPanel::StatsPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    StatsPanel::~StatsPanel() = default;

    void StatsPanel::OnActiveWorldChanged(World* /*InOld*/, World* /*InNew*/)
    {
        m_Display.Clear();
    }

    void StatsPanel::DrawContents()
    {
        const FrameStats& lStats = m_Context.Stats.GetFrameStats();

        // The graph gets every frame — it is the one thing here that SHOULD move at frame rate, and
        // a hitch that only lands between two refreshes must still show up as a spike.
        m_FrameHistory.Push(static_cast<float>(lStats.FrameMs));

        const double lNow = m_Context.Gui.GetTime();
        if (lNow - m_LastRefresh >= REFRESH_INTERVAL)
        {
            m_LastRefresh = lNow;

            m_Display.Update(lStats);

            m_ShownAvgMs = m_FrameHistory.Average();
            m_ShownMinMs = m_FrameHistory.Min();
            m_ShownMaxMs = m_FrameHistory.Max();
            m_ShownGpuMs = lStats.GpuMs;
        }

        DrawFrameTime();

        ImGui::Separator();

        DrawBreakdown();

        DrawCounters();
    }

    float StatsPanel::GraphCeilingMs() const
    {
        const float lNeeded = m_FrameHistory.Max() * 1.05f;

        if (lNeeded <= MIN_GRAPH_CEILING_MS) { return MIN_GRAPH_CEILING_MS; }

        // Whole frames, so an ageing spike steps the ceiling down once instead of sliding the whole
        // plot every frame as the maximum drifts.
        return std::ceil(lNeeded / k_FrameMs60) * k_FrameMs60;
    }

    void StatsPanel::DrawFrameTime()
    {
        // The AVERAGE, not the last frame: an instantaneous fps is unreadable at 60 Hz, and the
        // number an author acts on is the one the last two seconds sustained.
        const double lFps = m_ShownAvgMs > 0.f ? 1000.0 / m_ShownAvgMs : 0.0;

        ImGui::Text("%.0f FPS", lFps);
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2f ms avg)", m_ShownAvgMs);

        // GPU sits HERE and not in the breakdown below, because it runs ALONGSIDE the CPU rather
        // than inside the frame: a row in that table would be counted against the total and drive
        // the "Other" remainder negative. Negative means the device gave no reading.
        ImGui::SameLine();

        if (m_ShownGpuMs >= 0.0)
        {
            ImGui::TextDisabled("| GPU %.2f ms", m_ShownGpuMs);

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Device-side time for a recent frame, 1-2 frames behind:\n"
                                  "reading the query in its own frame would stall the pipeline.");
            }
        }
        else
        {
            ImGui::TextDisabled("| GPU --");

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("No reading yet, or this device has no timer support.");
            }
        }


        ImGui::PlotLines("##frametime",
                         m_FrameHistory.Data(),
                         static_cast<int>(m_FrameHistory.Count()),
                         static_cast<int>(m_FrameHistory.Offset()),
                         nullptr, 0.f, GraphCeilingMs(), ImVec2(-1.f, 60.f));

        ImGui::TextDisabled("min %.2f   max %.2f   (%u frames)",
                            m_ShownMinMs, m_ShownMaxMs, m_FrameHistory.Count());

        // No separate fixed-step readout: the FixedUpdate row's own "xN" is the step count, because
        // that scope sits inside the catch-up loop. A count stuck above 1 is the spiral of death.
    }

    void StatsPanel::DrawBreakdown()
    {
        if (m_Display.Scopes().empty())
        {
            ImGui::TextDisabled("No frame measured yet.");
            return;
        }

        // Which frame the rows below describe — they are a held snapshot, not the graph's newest
        // sample, and stating its total is what lets a reader check that the rows add up.
        ImGui::Text("Breakdown");
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2f ms frame)", m_Display.FrameMs());

        if (!ImGui::BeginTable("##breakdown", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            return;
        }

        ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("ms",    ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("%",     ImGuiTableColumnFlags_WidthFixed, 50.f);
        ImGui::TableHeadersRow();

        for (const ScopeSample& lSample : m_Display.Scopes())
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const float lIndent = static_cast<float>(lSample.Depth) * k_ScopeIndent;
            if (lIndent > 0.f) { ImGui::Indent(lIndent); }
            ImGui::TextUnformatted(lSample.Name != nullptr ? lSample.Name : "(unnamed)");

            // Only when it ran more than once — a "x1" on every row is noise. This is how a
            // catch-up frame reads: the fixed-step children say how many steps they served.
            if (lSample.Calls > 1)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("x%u", lSample.Calls);
            }

            if (lIndent > 0.f) { ImGui::Unindent(lIndent); }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", lSample.Milliseconds);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", PercentOfFrame(lSample.Milliseconds, m_Display.FrameMs()));
        }

        // What the engine did NOT measure: the host's event poll and, in this build, the editor's
        // whole UI pass. Named rather than hidden — the rows have to add up to the frame or the
        // panel is lying about where the time went. The editor's own cost is deliberately not
        // broken out: it is not the game's cost, and a shipped build has none of it.
        const double lUnmeasured = m_Display.UnmeasuredMs();

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Other");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Everything outside the engine's own phases:\n"
                              "the OS event poll, and the editor's UI pass.");
        }

        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%.2f", lUnmeasured);

        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("%.1f", PercentOfFrame(lUnmeasured, m_Display.FrameMs()));

        ImGui::EndTable();
    }

    void StatsPanel::DrawCounters()
    {
        const TDynArray<StatCounter>& lCounters = m_Display.Counters();

        if (lCounters.empty()) { return; }

        ImGui::Separator();
        ImGui::TextUnformatted("Counters");

        if (!ImGui::BeginTable("##counters", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            return;
        }

        ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value",   ImGuiTableColumnFlags_WidthFixed, 80.f);

        for (const StatCounter& lCounter : lCounters)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(lCounter.Name != nullptr ? lCounter.Name : "(unnamed)");

            ImGui::TableSetColumnIndex(1);

            // A frame that split its batch is ⑥'s bug made visible: Renderer2D sorts the CURRENT
            // batch only, so past one draw call the painter's algorithm no longer holds between
            // them. Flagged rather than explained in a comment nobody reads.
            const bool bSplitBatch = lCounter.Value > 1 && SameCounter(lCounter, "Draw Calls");

            if (bSplitBatch)
            {
                ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "%llu", lCounter.Value);
            }
            else
            {
                ImGui::Text("%llu", lCounter.Value);
            }

            if (bSplitBatch && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("The frame split into several batches.\n"
                                  "Draw order is only sorted WITHIN a batch, so sprites can\n"
                                  "overlap wrongly across the split. Check Quads (>1000 fills\n"
                                  "the buffer) and Texture Slots (16 exhausts the samplers).");
            }
        }

        ImGui::EndTable();
    }
}
