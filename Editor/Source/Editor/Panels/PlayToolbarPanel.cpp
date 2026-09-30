#include "Editor/Panels/PlayToolbarPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/PIE/PlayInEditor.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Extensions/EditorExtensionRegistrar.h"

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

    void PlayToolbarPanel::DrawContents()
    {
        PlayInEditor& lPIE = m_Context.PIE;

        // The buttons dispatch by tag, like the Play menu and the F-keys. Only the state is read from PIE.
        const auto lRun = [this](const OpaaxTag& InCommand)
        {
            m_Context.Extensions.Commands().Execute(InCommand, m_Context);
        };

        // Disabled rather than hidden: the buttons never move.
        ImGui::BeginDisabled(!lPIE.IsEdit());
        if (ImGui::Button("Play  (F5)")) { lRun(Tags::EDITOR_COMMAND_PLAY); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(lPIE.IsEdit());
        if (ImGui::Button(lPIE.IsPaused() ? "Resume  (F6)" : "Pause  (F6)")) { lRun(Tags::EDITOR_COMMAND_TOGGLE_PAUSE); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Step only from paused (from Playing it would race the frame).
        ImGui::BeginDisabled(!lPIE.IsPaused());
        if (ImGui::Button("Step  (F7)")) { lRun(Tags::EDITOR_COMMAND_STEP); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(lPIE.IsEdit());
        if (ImGui::Button("Stop  (F8)")) { lRun(Tags::EDITOR_COMMAND_STOP); }
        ImGui::EndDisabled();

        ImGui::Separator();

        // The state and its world: during Play two worlds share a name, the mode tells them apart.
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
    }
}
