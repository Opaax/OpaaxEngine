#include "Editor/Panels/ConfigPanel.h"

#include "Editor/EditorContext.h"

#include "Application/Services/IConfigSystem.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    ConfigPanel::ConfigPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    const IConfig* ConfigPanel::ResolveCurrent() const
    {
        if (const IConfig* lConfig = m_Context.Configs.FindConfig(m_Current))
        {
            return lConfig;
        }

        const TDynArray<TUniquePtr<IConfig>>& lConfigs = m_Context.Configs.GetConfigs();
        return lConfigs.empty() ? nullptr : lConfigs.front().get();
    }

    void ConfigPanel::DrawList()
    {
        const TDynArray<TUniquePtr<IConfig>>& lConfigs = m_Context.Configs.GetConfigs();

        if (lConfigs.empty())
        {
            ImGui::TextDisabled("(none)");
            return;
        }

        const IConfig* lCurrent = ResolveCurrent();

        for (const TUniquePtr<IConfig>& lConfig : lConfigs)
        {
            // CStr() into the intern pool — valid for the life of the process (I2), so ImGui can
            // hold it without a copy.
            if (ImGui::Selectable(lConfig->GetName().CStr(), lConfig.get() == lCurrent))
            {
                m_Current = lConfig->GetConfigTypeID();
            }
        }
    }

    void ConfigPanel::DrawCurrent(const IConfig& InConfig)
    {
        ImGui::TextUnformatted(InConfig.GetName().CStr());
        ImGui::TextDisabled("%s", InConfig.FileName());
        ImGui::Separator();

        // Re-serialized every frame rather than cached: a cache would need an invalidation nobody
        // owns, and a config the panel showed as stale would be worse than useless.
        const OpaaxString lText = InConfig.ToText();

        if (m_Reported != InConfig.GetConfigTypeID())
        {
            m_Reported = InConfig.GetConfigTypeID();
            OPAAX_LOG(LogConfigPanel, Info, "Showing '{}' ({}, {} bytes)",
                      InConfig.GetName(), InConfig.FileName(), lText.GetLength());
        }

        if (ImGui::BeginChild("##values", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::TextUnformatted(lText.CStr());
        }

        ImGui::EndChild();
    }

    void ConfigPanel::DrawContents()
    {
        if (ImGui::BeginChild("##list", ImVec2(LIST_WIDTH, 0.f), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders))
        {
            DrawList();
        }

        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("##current", ImVec2(0.f, 0.f)))
        {
            if (const IConfig* lCurrent = ResolveCurrent())
            {
                DrawCurrent(*lCurrent);
            }
            else
            {
                ImGui::TextDisabled("No config registered.");
            }
        }

        ImGui::EndChild();
    }
}
