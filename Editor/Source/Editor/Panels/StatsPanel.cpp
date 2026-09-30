#include "Editor/Panels/StatsPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/UI/IEditorGui.h"   // GetTime

#include "Core/Profiling/Profiler.h"

#include <imgui.h>

#include <cmath>

using namespace Opaax;

namespace
{
    /** Pixels of indent per nesting level in the scope tree. */
    constexpr float k_ScopeIndent = 14.f;

    /** One 60 Hz frame. The graph's ceiling steps in these. */
    constexpr float k_FrameMs60 = 1000.f / 60.f;

    /** Percentage of the frame InMilliseconds is. Zero for an empty frame. */
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
        const FrameStats& lStats = Profiler::Get().GetFrameStats();

        // The graph gets every frame (a spike between two refreshes must still show).
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

        // Whole frames, so the ceiling steps down once instead of sliding every frame.
        return std::ceil(lNeeded / k_FrameMs60) * k_FrameMs60;
    }

    void StatsPanel::DrawFrameTime()
    {
        // The average over the history (an instant fps is unreadable).
        const double lFps = m_ShownAvgMs > 0.f ? 1000.0 / m_ShownAvgMs : 0.0;

        ImGui::Text("%.0f FPS", lFps);
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2f ms avg)", m_ShownAvgMs);

        // GPU time runs alongside the CPU, so it is shown here, not in the breakdown (it would push
        // "Other" negative). Negative means no reading.
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

        // The FixedUpdate row's "xN" is the step count. A count stuck above 1 is the spiral of death.
    }

    void StatsPanel::DrawBreakdown()
    {
        if (m_Display.Scopes().empty())
        {
            ImGui::TextDisabled("No frame measured yet.");
            return;
        }

        // Which frame the rows describe (a held snapshot), and its total, so the rows can be checked.
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

            // Only when it ran more than once (the fixed-step children show the step count).
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

        // What the engine did not measure: the event poll and the editor's UI pass. Shown so the rows
        // add up to the frame.
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

            // All counters look the same (no warning colour: a split frame only costs time).
            ImGui::Text("%llu", lCounter.Value);
        }

        ImGui::EndTable();
    }
}
