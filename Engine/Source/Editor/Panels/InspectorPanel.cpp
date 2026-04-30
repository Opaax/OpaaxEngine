#include "InspectorPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Editor/Inspector/ComponentDrawerRegistry.h"

namespace Opaax::Editor
{
    void InspectorPanel::Draw(World* InWorld, EntityID InSelected)
    {
        ImGui::Begin("Inspector");

        if (!InWorld || InSelected == ENTITY_NONE)
        {
            ImGui::TextDisabled("No entity selected.");
            ImGui::End();
            return;
        }

        if (!InWorld->IsValid(InSelected))
        {
            ImGui::TextDisabled("Entity is no longer valid.");
            ImGui::End();
            return;
        }

        ComponentDrawerRegistry::DrawAll(*InWorld, InSelected);

        ImGui::Separator();

        if (ImGui::Button("+ Add Component", ImVec2(-1.f, 0.f)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            ComponentDrawerRegistry::DrawAddComponentMenu(*InWorld, InSelected);
            ImGui::EndPopup();
        }

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
