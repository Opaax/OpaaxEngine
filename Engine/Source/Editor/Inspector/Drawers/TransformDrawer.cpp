#include "TransformDrawer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "ECS/Components/TransformComponent.h"

namespace Opaax::Editor
{
    bool TransformDrawer::HasComponent(World& InWorld, EntityID InEntity) const
    {
        return InWorld.GetComponent<ECS::TransformComponent>(InEntity) != nullptr;
    }

    void TransformDrawer::Draw(World& InWorld, EntityID InEntity)
    {
        auto* lT = InWorld.GetComponent<ECS::TransformComponent>(InEntity);
        if (!lT)
        {
            return;
        }

        const bool bOpen = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);

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

    void TransformDrawer::Add(World& InWorld, EntityID InEntity)
    {
        InWorld.AddComponent<ECS::TransformComponent>(InEntity);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
