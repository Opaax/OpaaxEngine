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

        // The buttons DISPATCH BY TAG, exactly as the Play menu and the reserved F-keys do. They
        // used to call PlayInEditor directly, which made three front-ends onto one state machine
        // three separate call sites to keep correct; only the STATE is still read from PIE here,
        // because that is what a button has to look like.
        const auto lRun = [this](const OpaaxTag& InCommand)
        {
            m_Context.Extensions.Commands().Execute(InCommand, m_Context);
        };

        // Disabled rather than hidden: the set of controls never moves under the cursor, and a
        // greyed button still says what the editor CAN do next.
        ImGui::BeginDisabled(!lPIE.IsEdit());
        if (ImGui::Button("Play  (F5)")) { lRun(Tags::EDITOR_COMMAND_PLAY); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(lPIE.IsEdit());
        if (ImGui::Button(lPIE.IsPaused() ? "Resume  (F6)" : "Pause  (F6)")) { lRun(Tags::EDITOR_COMMAND_TOGGLE_PAUSE); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Step only means something from a stopped clock — from Playing it would race the frame.
        ImGui::BeginDisabled(!lPIE.IsPaused());
        if (ImGui::Button("Step  (F7)")) { lRun(Tags::EDITOR_COMMAND_STEP); }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(lPIE.IsEdit());
        if (ImGui::Button("Stop  (F8)")) { lRun(Tags::EDITOR_COMMAND_STOP); }
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
    }
}
