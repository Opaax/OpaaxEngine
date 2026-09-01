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
            // CStr() into the intern pool — valid for the life of the process (I2), so ImGui can
            // hold it without a copy.
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

        // A registered drawer folds the config's own property list; anything else falls back to the
        // json view, so a config nobody registered is still readable rather than blank.
        const bool lDrawn = m_Context.Extensions.ConfigDrawers().DrawFirst(InConfig, m_Context.Gui);

        // Re-serialized every frame rather than cached: it is both what the fallback shows and how
        // dirty is DERIVED below, and a cache here would need an invalidation nobody owns.
        const OpaaxString lText = InConfig.ToText();

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
        // DIRTY IS DERIVED, not flagged: what the config serializes to now, against what it
        // serialized to when this panel last showed or saved it. Nothing has to remember to mark
        // anything — the EditorLevelDocument shape, one scale down.
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
