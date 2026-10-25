#include "Editor/Panels/ConfigPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Properties/WidgetPropertyVisitor.h"

#include "Application/Services/IConfigSystem.h"
#include "Engine/Config/Config.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    ConfigPanel::ConfigPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    Config* ConfigPanel::ResolveCurrent() const
    {
        if (Config* lConfig = m_Current != nullptr ? m_Context.Configs.FindConfig(*m_Current) : nullptr)
        {
            return lConfig;
        }

        const TDynArray<TStrongObjectPtr<Config>>& lConfigs = m_Context.Configs.GetConfigs();
        return lConfigs.empty() ? nullptr : lConfigs.front().Get();
    }

    void ConfigPanel::DrawList()
    {
        const TDynArray<TStrongObjectPtr<Config>>& lConfigs = m_Context.Configs.GetConfigs();

        if (lConfigs.empty())
        {
            ImGui::TextDisabled("(none)");
            return;
        }

        const Config* lCurrent = ResolveCurrent();

        for (const TStrongObjectPtr<Config>& lConfig : lConfigs)
        {
            // CStr() points into the intern pool (valid for the whole process).
            if (ImGui::Selectable(lConfig->GetName().CStr(), lConfig.Get() == lCurrent))
            {
                m_Current = &lConfig->GetClass();
            }
        }
    }

    void ConfigPanel::DrawCurrent(Config& InConfig)
    {
        ImGui::TextUnformatted(InConfig.GetName().CStr());
        ImGui::TextDisabled("%s", InConfig.FileName());
        ImGui::Separator();

        // Drawn from the values' properties, like any object; without them, the text below.
        WidgetPropertyVisitor lWidgets(m_Context.Widgets);
        const bool            lDrawn = InConfig.VisitValues(lWidgets);

        // Serialized every frame: shown by the fallback, and used for the dirty state.
        const OpaaxString lText = InConfig.ToText();

        // A drag is announced on release (IsAnyItemActive holds it).
        if (m_ChangeTracker.Update(InConfig.GetName(), lText, ImGui::IsAnyItemActive()))
        {
            InConfig.NotifyChanged();
        }

        if (m_Reported != &InConfig)
        {
            m_Reported = &InConfig;
            m_Baseline = lText;

            OPAAX_LOG(LogConfigPanel, Info, "Showing '{}' ({}, {})", InConfig.GetName(), InConfig.FileName(),
                      lDrawn ? "properties" : "no properties — json view");
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

    void ConfigPanel::DrawSaveBar(Config& InConfig, const OpaaxString& InCurrentText)
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
        ImGui::SameLine();

        // Not saved: the values go back to their defaults, and Save writes them.
        if (ImGui::Button("Reset to Defaults"))
        {
            InConfig.ResetToDefaults();
            OPAAX_LOG(LogConfigPanel, Info, "Reset '{}' to its defaults (not saved)", InConfig.GetName());
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Every value back to its built-in default. Save writes them.");
        }

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
            if (Config* lCurrent = ResolveCurrent())
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
