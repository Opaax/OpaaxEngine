#include "Editor/Panels/ConfigPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"

#include "Application/Services/IConfigSystem.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    ConfigPanel::ConfigPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    IConfig* ConfigPanel::ResolveCurrent() const
    {
        if (IConfig* lConfig = m_Context.Configs.FindConfig(m_Current))
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
            // CStr() points into the intern pool (valid for the whole process).
            if (ImGui::Selectable(lConfig->GetName().CStr(), lConfig.get() == lCurrent))
            {
                m_Current = lConfig->GetConfigTypeID();
            }
        }
    }

    void ConfigPanel::DrawCurrent(IConfig& InConfig)
    {
        ImGui::TextUnformatted(InConfig.GetName().CStr());
        ImGui::TextDisabled("%s", InConfig.FileName());
        ImGui::Separator();

        // A registered drawer draws the config's properties; otherwise show its JSON text.
        const bool lDrawn = m_Context.Extensions.ConfigDrawers().DrawFirst(InConfig, m_Context.Widgets, m_Context);

        // Serialized every frame: shown by the fallback, and used for the dirty state.
        const OpaaxString lText = InConfig.ToText();

        // A drag is announced on release (IsAnyItemActive holds it).
        if (m_ChangeTracker.Update(InConfig.GetConfigTypeID(), lText, ImGui::IsAnyItemActive()))
        {
            InConfig.NotifyChanged();
        }

        if (m_Reported != InConfig.GetConfigTypeID())
        {
            m_Reported = InConfig.GetConfigTypeID();
            m_Baseline = lText;

            OPAAX_LOG(LogConfigPanel, Info, "Showing '{}' ({}, {})", InConfig.GetName(), InConfig.FileName(),
                      lDrawn ? "properties" : "no drawer — json view");
        }

        DrawSaveBar(InConfig, lText);

        if (lDrawn) { return; }

        if (ImGui::BeginChild("##values", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::TextUnformatted(lText.CStr());
        }

        ImGui::EndChild();
    }

    void ConfigPanel::DrawSaveBar(IConfig& InConfig, const OpaaxString& InCurrentText)
    {
        // Dirty is derived: the current text against the text when last shown or saved.
        const bool lDirty = InCurrentText != m_Baseline;

        ImGui::Separator();
        ImGui::BeginDisabled(!lDirty);

        if (ImGui::Button("Save"))
        {
            if (InConfig.Save())
            {
                m_Baseline = InCurrentText;
                OPAAX_LOG(LogConfigPanel, Info, "Saved '{}' to {}", InConfig.GetName(), InConfig.FileName());
            }
            else
            {
                OPAAX_LOG(LogConfigPanel, Warn, "Could not write '{}' — is it read-only?", InConfig.FileName());
            }
        }

        ImGui::EndDisabled();

        if (lDirty)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("unsaved changes");
        }

        ImGui::Separator();
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
            if (IConfig* lCurrent = ResolveCurrent())
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
