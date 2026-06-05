#include "RigidbodyDrawer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "ECS/Components/RigidbodyComponent.h"

namespace Opaax::Editor
{
    bool RigidbodyDrawer::HasComponent(World& InWorld, EntityID InEntity) const
    {
        return InWorld.GetComponent<ECS::RigidbodyComponent>(InEntity) != nullptr;
    }

    void RigidbodyDrawer::Draw(World& InWorld, EntityID InEntity)
    {
        auto* lRb = InWorld.GetComponent<ECS::RigidbodyComponent>(InEntity);
        if (!lRb) { return; }

        const bool bOpen = ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SmallButton("X##RemoveRigidbody"))
        {
            InWorld.RemoveComponent<ECS::RigidbodyComponent>(InEntity);
            return;
        }

        if (!bOpen) { return; }

        static const char* lBodyTypeNames[] = { "Static", "Kinematic", "Dynamic" };
        int lType = static_cast<int>(lRb->Type);
        if (ImGui::Combo("Body Type", &lType, lBodyTypeNames, IM_ARRAYSIZE(lBodyTypeNames)))
        {
            lRb->Type = static_cast<EBodyType>(lType);
        }

        ImGui::DragFloat ("Gravity Scale",   &lRb->GravityScale,   0.01f, 0.f, 0.f, "%.2f");
        ImGui::Checkbox  ("Fixed Rotation",  &lRb->FixedRotation);
        ImGui::DragFloat ("Linear Damping",  &lRb->LinearDamping,  0.01f, 0.f, 0.f, "%.2f");
        ImGui::DragFloat ("Angular Damping", &lRb->AngularDamping, 0.01f, 0.f, 0.f, "%.2f");
    }

    void RigidbodyDrawer::Add(World& InWorld, EntityID InEntity)
    {
        InWorld.AddComponent<ECS::RigidbodyComponent>(InEntity);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
