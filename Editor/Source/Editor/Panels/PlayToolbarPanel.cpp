#include "Editor/Panels/PlayToolbarPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/PlayInEditor.h"

#include "World/World.h"
#include "World/WorldManager.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    PlayToolbarPanel::PlayToolbarPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    PlayToolbarPanel::~PlayToolbarPanel() = default;

    void PlayToolbarPanel::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(360.f, 90.f), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Title.CStr());

        PlayInEditor& lPIE = m_Context.PIE;

        // Disabled rather than hidden: the set of controls never moves under the cursor, and a
        // greyed button still says what the editor CAN do next.
        ImGui::BeginDisabled(!lPIE.IsEdit());
        if (ImGui::Button("Play  (F5)")) { lPIE.Play(); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(lPIE.IsEdit());
        if (ImGui::Button(lPIE.IsPaused() ? "Resume  (F6)" : "Pause  (F6)")) { lPIE.TogglePause(); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Step only means something from a stopped clock — from Playing it would race the frame.
        ImGui::BeginDisabled(!lPIE.IsPaused());
        if (ImGui::Button("Step  (F7)")) { lPIE.Step(); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(lPIE.IsEdit());
        if (ImGui::Button("Stop  (F8)")) { lPIE.Stop(); }
        ImGui::EndDisabled();

        ImGui::Separator();

        // The state and the world it applies to, together: during PIE two worlds exist and share a
        // name, so the mode is what tells the reader which one is live.
        const World* lActive = m_Context.Worlds.GetActiveWorld();

        ImGui::Text("%s", ToString(lPIE.GetState()));
        ImGui::SameLine();

        if (lActive != nullptr)
        {
            ImGui::TextDisabled("— world '%s' (%s)", lActive->GetName().CStr(), ToString(lActive->GetMode()));
        }
        else
        {
            ImGui::TextDisabled("— no active world");
        }

        ImGui::End();
    }
}
