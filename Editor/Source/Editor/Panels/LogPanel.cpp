#include "Editor/Panels/LogPanel.h"

#include "Core/Log/Logger.h"

#include <imgui.h>

#include <cstdio>
#include <ctime>

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    const char* LevelLabel(const ELogLevel InLevel)
    {
        switch (InLevel)
        {
        case ELogLevel::Trace:    return "trace";
        case ELogLevel::Info:     return "info";
        case ELogLevel::Warn:     return "warn";
        case ELogLevel::Error:    return "error";
        case ELogLevel::Critical: return "critical";
        }
        return "?";
    }

    /** The console's colours, so a line reads the same in both places. Info keeps the theme's text. */
    ImVec4 LevelColour(const ELogLevel InLevel)
    {
        switch (InLevel)
        {
        case ELogLevel::Trace:    return ImVec4(0.55f, 0.55f, 0.55f, 1.f);
        case ELogLevel::Warn:     return ImVec4(1.f, 0.8f, 0.3f, 1.f);
        case ELogLevel::Error:    return ImVec4(1.f, 0.4f, 0.4f, 1.f);
        case ELogLevel::Critical: return ImVec4(1.f, 0.25f, 0.25f, 1.f);
        default:                  return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }
    }

    /** The colour a level BUTTON wears — its lines' colour. */
    ImVec4 LevelColour(const ELogLevelFilter InLevel)
    {
        switch (InLevel)
        {
        case ELogLevelFilter::Trace: return LevelColour(ELogLevel::Trace);
        case ELogLevelFilter::Warn:  return LevelColour(ELogLevel::Warn);
        case ELogLevelFilter::Error: return LevelColour(ELogLevel::Error);
        default:                     return LevelColour(ELogLevel::Info);
        }
    }

    size_t LevelIndex(const ELogLevel InLevel)
    {
        return static_cast<size_t>(ToLevelFilter(InLevel));
    }

    /** HH:MM:SS local time — the date is the file's business, not a panel column's. */
    void FormatTime(const std::chrono::system_clock::time_point InTime, char (&OutText)[16])
    {
        const std::time_t lTime = std::chrono::system_clock::to_time_t(InTime);
        std::tm           lLocal{};
        localtime_s(&lLocal, &lTime);
        std::strftime(OutText, sizeof(OutText), "%H:%M:%S", &lLocal);
    }
}

namespace Opaax::Editor
{
    LogPanel::LogPanel(EditorContext& /*InContext*/)
    {
    }

    LogPanel::~LogPanel() = default;

    void LogPanel::DrawContents()
    {
        PullNewLines();
        DrawToolbar();
        DrawLines();
    }

    void LogPanel::PullNewLines()
    {
        m_Incoming.clear();
        m_LastSequence = Logger::Get().CopyHistorySince(m_LastSequence, m_Incoming);

        if (m_Incoming.empty()) { return; }

        // The Logger dropped lines this panel never saw (hidden while more than MAX_LINES arrived).
        // What is held here is older than a gap, so start over from the new lines — m_Entries must
        // stay contiguous for EntryAt.
        if (!m_Entries.empty() && m_Incoming.front().Sequence != m_Entries.back().Sequence + 1)
        {
            ClearLines();
        }

        for (LogEntry& lEntry : m_Incoming)
        {
            ++m_LevelCounts[LevelIndex(lEntry.Level)];

            if (m_Filter.Passes(lEntry))
            {
                m_Shown.push_back(lEntry.Sequence);
            }

            m_Entries.push_back(std::move(lEntry));
        }

        while (m_Entries.size() > MAX_LINES)
        {
            const LogEntry& lOldest = m_Entries.front();

            --m_LevelCounts[LevelIndex(lOldest.Level)];

            if (!m_Shown.empty() && m_Shown.front() == lOldest.Sequence)
            {
                m_Shown.pop_front();
            }

            m_Entries.pop_front();
        }
    }

    void LogPanel::DrawToolbar()
    {
        if (ImGui::Button("Clear"))
        {
            ClearLines();
        }

        bool bFilterChanged = false;

        for (size_t lLevel = 0; lLevel < LEVEL_COUNT; ++lLevel)
        {
            ImGui::SameLine();
            bFilterChanged |= DrawLevelToggle(static_cast<ELogLevelFilter>(lLevel));
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::InputTextWithHint("##search", "Search messages", m_SearchBuffer, sizeof(m_SearchBuffer)))
        {
            m_Filter.Search = m_SearchBuffer;
            bFilterChanged  = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%u / %u lines", static_cast<Uint32>(m_Shown.size()), static_cast<Uint32>(m_Entries.size()));

        if (bFilterChanged)
        {
            Refilter();
        }
    }

    bool LogPanel::DrawLevelToggle(const ELogLevelFilter InLevel)
    {
        const size_t lIndex = static_cast<size_t>(InLevel);
        bool&        bShown = m_Filter.ShowLevel[lIndex];

        // Off reads as off: dimmed text on a plain frame. The "###" id keeps the button the same
        // widget while its count changes.
        ImGui::PushStyleColor(ImGuiCol_Text, bShown ? LevelColour(InLevel) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (!bShown)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        }

        char lLabel[48];
        std::snprintf(lLabel, sizeof(lLabel), "%s %u###level%zu", ToString(InLevel), m_LevelCounts[lIndex], lIndex);
        const bool bClicked = ImGui::Button(lLabel);

        ImGui::PopStyleColor(bShown ? 1 : 2);

        if (bClicked)
        {
            bShown = !bShown;
        }
        return bClicked;
    }

    void LogPanel::DrawLines()
    {
        constexpr ImGuiTableFlags lFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg
                                         | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

        if (!ImGui::BeginTable("##lines", 3, lFlags))
        {
            return;
        }

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("00:00:00").x);
        ImGui::TableSetupColumn("Level",   ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("critical").x);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // Read BEFORE this frame's rows grow the content: at the bottom last frame = follow the new lines.
        const bool bFollow = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

        ImGuiListClipper lClipper;
        lClipper.Begin(static_cast<int>(m_Shown.size()));
        while (lClipper.Step())
        {
            for (int lRow = lClipper.DisplayStart; lRow < lClipper.DisplayEnd; ++lRow)
            {
                const LogEntry& lEntry = EntryAt(m_Shown[static_cast<size_t>(lRow)]);

                char lTime[16];
                FormatTime(lEntry.Time, lTime);

                ImGui::TableNextRow();
                ImGui::PushStyleColor(ImGuiCol_Text, LevelColour(lEntry.Level));

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(lTime);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(LevelLabel(lEntry.Level));
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(lEntry.Message.CStr());

                ImGui::PopStyleColor();
            }
        }

        if (bFollow)
        {
            ImGui::SetScrollHereY(1.f);
        }

        ImGui::EndTable();
    }

    void LogPanel::Refilter()
    {
        m_Shown.clear();

        for (const LogEntry& lEntry : m_Entries)
        {
            if (m_Filter.Passes(lEntry))
            {
                m_Shown.push_back(lEntry.Sequence);
            }
        }
    }

    void LogPanel::ClearLines()
    {
        m_Entries.clear();
        m_Shown.clear();
        m_LevelCounts.fill(0);
    }

    const LogEntry& LogPanel::EntryAt(const Uint64 InSequence) const
    {
        return m_Entries[static_cast<size_t>(InSequence - m_Entries.front().Sequence)];
    }
}
