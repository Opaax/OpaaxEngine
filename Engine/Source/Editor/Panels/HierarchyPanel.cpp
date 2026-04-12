#include "HierarchyPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include <entt/entt.hpp>
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    void HierarchyPanel::Draw(SceneManager* InSceneManager)
    {
        ImGui::Begin("Hierarchy");

        // No scene active
        if (!InSceneManager || !InSceneManager->GetActiveScene())
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        Scene*      lScene    = InSceneManager->GetActiveScene();
        World& lWorld    = lScene->GetWorld();
        auto&       lRegistry = lWorld.GetRegistry();

        // Scene name as header
        ImGui::SeparatorText(lScene->GetName().CStr());

        // Entity count
        ImGui::TextDisabled("%u entities", lWorld.GetEntityCount());
        ImGui::Separator();

        // List all entities with a TagComponent
        auto lView = lRegistry.view<const ECS::TagComponent>();

        for (auto lEntity : lView)
        {
            const auto& lTag = lView.get<ECS::TagComponent>(lEntity);
            const bool  bSelected = (lEntity == m_SelectedEntity);

            // Build label — tag + entity id for uniqueness
            char lLabel[128];
            snprintf(lLabel, sizeof(lLabel), "%s##%u",
                lTag.Tag.ToString().CStr(),
                static_cast<Uint32>(lEntity));

            if (ImGui::Selectable(lLabel, bSelected))
            {
                m_SelectedEntity = bSelected ? ENTITY_NONE : lEntity;
            }

            // Right-click context menu per entity
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Delete Entity"))
                {
                    // NOTE: Defer destruction — we are iterating the registry.
                    //   Flag for deletion, process after the loop.
                    lWorld.DestroyEntity(lEntity);

                    if (m_SelectedEntity == lEntity)
                    {
                        ClearSelection();
                    }
                    ImGui::EndPopup();
                    break; // Registry invalidated — stop iteration
                }
                ImGui::EndPopup();
            }
        }

        ImGui::Separator();

        // Create entity button
        if (ImGui::Button("+ Add Entity", ImVec2(-1.f, 0.f)))
        {
            const EntityID lNew = lWorld.CreateEntity("NewEntity");
            m_SelectedEntity = lNew;
        }

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR