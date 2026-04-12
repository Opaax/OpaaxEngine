#include "InspectorPanel.h"

#include "ECS/Components/SpriteComponent.h"


#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/TagComponent.h"
#include "Core/Log/OpaaxLog.h"

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

        DrawTagComponent      (*InWorld, InSelected);
        DrawTransformComponent(*InWorld, InSelected);
        DrawSpriteComponent   (*InWorld, InSelected);

        // TODO: DrawPhysicsComponent, DrawScriptComponent, etc. — add as components are added.

        ImGui::Separator();

        // Add component menu
        if (ImGui::Button("+ Add Component", ImVec2(-1.f, 0.f)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (!InWorld->HasComponent<ECS::TransformComponent>(InSelected))
            {
                if (ImGui::MenuItem("Transform"))
                {
                    InWorld->AddComponent<ECS::TransformComponent>(InSelected);
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!InWorld->HasComponent<ECS::SpriteComponent>(InSelected))
            {
                if (ImGui::MenuItem("Sprite"))
                {
                    InWorld->AddComponent<ECS::SpriteComponent>(InSelected);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }

        ImGui::End();
    }

    // =============================================================================
    // TagComponent
    // =============================================================================
    void InspectorPanel::DrawTagComponent(World& InWorld, EntityID InEntity)
    {
        auto* lTag = InWorld.GetComponent<ECS::TagComponent>(InEntity);
        if (!lTag) { return; }

        if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char lBuffer[256];
            strncpy_s(lBuffer, sizeof(lBuffer), lTag->Tag.ToString().CStr(), _TRUNCATE);

            if (ImGui::InputText("##Tag", lBuffer, sizeof(lBuffer)))
            {
                lTag->Tag = OpaaxStringID(lBuffer);
            }
        }
    }

    // =============================================================================
    // TransformComponent
    // =============================================================================
    void InspectorPanel::DrawTransformComponent(World& InWorld, EntityID InEntity)
    {
        auto* lT = InWorld.GetComponent<ECS::TransformComponent>(InEntity);
        if (!lT) { return; }

        bool bOpen = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);

        // Remove component button — aligned right
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SmallButton("X##RemoveTransform"))
        {
            InWorld.RemoveComponent<ECS::TransformComponent>(InEntity);
            return;
        }

        if (!bOpen) { return; }

        ImGui::DragFloat2("Position", &lT->Position.x, 1.f);
        ImGui::DragFloat2("Scale",    &lT->Scale.x,    0.5f, 0.01f, 10000.f);
        ImGui::DragFloat ("Rotation", &lT->Rotation,   0.01f);
        ImGui::DragFloat ("Z Order",  &lT->ZOrder,     0.1f);
    }

    // =============================================================================
    // SpriteComponent
    // =============================================================================
    void InspectorPanel::DrawSpriteComponent(World& InWorld, EntityID InEntity)
    {
        auto* lS = InWorld.GetComponent<ECS::SpriteComponent>(InEntity);
        if (!lS) { return; }

        bool bOpen = ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen);

        // Remove component button — aligned right
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SmallButton("X##RemoveSprite"))
        {
            InWorld.RemoveComponent<ECS::SpriteComponent>(InEntity);
            return;
        }

        if (!bOpen) { return; }

        // Texture path — read only for now
        // TODO: drag & drop from asset browser (M7 étape 7)
        const char* lPath = lS->Texture.IsValid()
            ? lS->Texture.GetID().ToString().CStr()
            : "None";

        ImGui::LabelText("Texture", "%s", lPath);

        ImGui::ColorEdit4("Color",  &lS->Color.r);
        ImGui::DragFloat2("UV Min", &lS->UVMin.x, 0.01f, 0.f, 1.f);
        ImGui::DragFloat2("UV Max", &lS->UVMax.x, 0.01f, 0.f, 1.f);
        ImGui::Checkbox  ("Visible", &lS->Visible);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR