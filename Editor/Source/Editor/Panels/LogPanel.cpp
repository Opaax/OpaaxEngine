#include "Editor/Panels/LogPanel.h"

#include "Core/Log/Logger.h"

#include <imgui.h>

#include <ctime>
#include <iterator>

using namespace Opaax;

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

        if (ImGui::Button("Clear"))
        {
            m_Entries.clear();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%u lines", static_cast<Uint32>(m_Entries.size()));

        DrawLines();
    }

    void LogPanel::PullNewLines()
    {
        m_Incoming.clear();
        m_LastSequence = Logger::Get().CopyHistorySince(m_LastSequence, m_Incoming);

        m_Entries.insert(m_Entries.end(), std::make_move_iterator(m_Incoming.begin()),
                         std::make_move_iterator(m_Incoming.end()));

        while (m_Entries.size() > MAX_LINES)
        {
            m_Entries.pop_front();
        }
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
        lClipper.Begin(static_cast<int>(m_Entries.size()));
        while (lClipper.Step())
        {
            for (int lRow = lClipper.DisplayStart; lRow < lClipper.DisplayEnd; ++lRow)
            {
                const LogEntry& lEntry = m_Entries[static_cast<size_t>(lRow)];

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
}
