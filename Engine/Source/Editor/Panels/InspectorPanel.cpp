#include "InspectorPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Editor/Events/EditorEvents.h"
#include "Editor/Inspector/ComponentDrawerRegistry.h"

namespace Opaax::Editor
{
    void InspectorPanel::OnSubscribe(EditorEventBus& InBus)
    {
        m_SelectionToken = InBus.Subscribe<OnEntitySelectedEvent>(
            [this](const OnEntitySelectedEvent& InEvent)
            {
                m_SelectedEntity = InEvent.GetEntity();
                OPAAX_CORE_INFO("Inspector Panel - New Entity selected: {}", "Implement Entity::GetName");
            });

        m_NewSceneToken = InBus.Subscribe<OnNewSceneEvent>(
            [this](const OnNewSceneEvent&)
            {
                OPAAX_CORE_INFO("Inspector Panel - OnNewSceneEvent received, clearing cached entity");
                m_SelectedEntity = ENTITY_NONE;
            });
    }

    void InspectorPanel::Draw(World& InWorld)
    {
        ImGui::Begin("Inspector");

        if (m_SelectedEntity == ENTITY_NONE)
        {
            ImGui::TextDisabled("No entity selected.");
            ImGui::End();
            return;
        }

        // IsValid gate stays — the cache is advisory; an entity destroyed by a
        // path that doesn't publish (game system, future code) would otherwise
        // dereference an invalid handle.
        if (!InWorld.IsValid(m_SelectedEntity))
        {
            ImGui::TextDisabled("Entity is no longer valid.");
            ImGui::End();
            return;
        }

        ComponentDrawerRegistry::DrawAll(InWorld, m_SelectedEntity);

        ImGui::Separator();

        if (ImGui::Button("+ Add Component", ImVec2(-1.f, 0.f)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            ComponentDrawerRegistry::DrawAddComponentMenu(InWorld, m_SelectedEntity);
            ImGui::EndPopup();
        }

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
