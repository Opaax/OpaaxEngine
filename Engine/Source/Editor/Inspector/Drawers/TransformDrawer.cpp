#include "TransformDrawer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include <cstdio>

#include "ECS/Components/ParentComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/UuidComponent.h"
#include "ECS/Hierarchy.h"

namespace Opaax::Editor
{
    bool TransformDrawer::HasComponent(World& InWorld, EntityID InEntity) const
    {
        return InWorld.GetComponent<ECS::TransformComponent>(InEntity) != nullptr;
    }

    void TransformDrawer::Draw(World& InWorld, EntityID InEntity)
    {
        if (!HasComponent(InWorld, InEntity))
        {
            return;
        }

        auto* lT = InWorld.GetComponent<ECS::TransformComponent>(InEntity);
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

        // Rotation: stored as radians, edited as degrees via SliderAngle.
        ImGui::SliderAngle("Rotation", &lT->Rotation, -180.f, 180.f);

        ImGui::DragFloat ("Z Order",  &lT->ZOrder,     0.1f);

        // ---- Parent picker ----
        DrawParentPicker(InWorld, InEntity);

        // ---- Read-only world transform ----
        if (ImGui::TreeNodeEx("World (read-only)", ImGuiTreeNodeFlags_None))
        {
            const ECS::Hierarchy::WorldTransform lWT =
                ECS::Hierarchy::GetWorldTransform(InWorld, InEntity);
            ImGui::Text("Position : (%.2f, %.2f)", lWT.Position.x, lWT.Position.y);
            ImGui::Text("Scale    : (%.2f, %.2f)", lWT.Scale.x,    lWT.Scale.y);
            ImGui::Text("Rotation : %.2f deg",     lWT.Rotation * 57.2957795f);
            ImGui::Text("Z Order  : %.2f",         lWT.ZOrder);
            ImGui::TreePop();
        }
    }

    void TransformDrawer::Add(World& InWorld, EntityID InEntity)
    {
        InWorld.AddComponent<ECS::TransformComponent>(InEntity);
    }

    void TransformDrawer::DrawParentPicker(World& InWorld, EntityID InEntity)
    {
        const auto* lParentComp = InWorld.GetComponent<ECS::ParentComponent>(InEntity);
        const EntityID lCurrentParent = lParentComp ? lParentComp->Parent : ENTITY_NONE;

        // Build preview label for the current selection.
        char lPreview[160];
        if (lCurrentParent == ENTITY_NONE)
        {
            std::snprintf(lPreview, sizeof(lPreview), "<None>");
        }
        else if (const auto* lTag = InWorld.GetComponent<ECS::TagComponent>(lCurrentParent))
        {
            std::snprintf(lPreview, sizeof(lPreview), "%s", lTag->Tag.ToString().CStr());
        }
        else
        {
            std::snprintf(lPreview, sizeof(lPreview), "<id %u>",
                static_cast<Uint32>(lCurrentParent));
        }

        bool bRejected = false;

        if (ImGui::BeginCombo("Parent", lPreview))
        {
            // <None> entry
            const bool bNoneSelected = (lCurrentParent == ENTITY_NONE);
            if (ImGui::Selectable("<None>", bNoneSelected))
            {
                ECS::Hierarchy::ClearParent(InWorld, InEntity);
            }

            ImGui::Separator();

            auto& lRegistry = InWorld.GetRegistry();
            auto lView = lRegistry.view<const ECS::TagComponent>();

            for (auto lCandidate : lView)
            {
                if (lCandidate == InEntity) { continue; }                                  // self
                if (ECS::Hierarchy::IsDescendantOf(InWorld, lCandidate, InEntity)) { continue; } // would cycle

                const auto& lTag = lView.get<const ECS::TagComponent>(lCandidate);

                char lLabel[160];
                std::snprintf(lLabel, sizeof(lLabel), "%s##%u",
                    lTag.Tag.ToString().CStr(),
                    static_cast<Uint32>(lCandidate));

                const bool bSelected = (lCandidate == lCurrentParent);
                if (ImGui::Selectable(lLabel, bSelected))
                {
                    if (!ECS::Hierarchy::SetParent(InWorld, InEntity, lCandidate))
                    {
                        bRejected = true;
                    }
                }
            }
            ImGui::EndCombo();
        }

        if (bRejected)
        {
            ImGui::OpenPopup("CycleRejected");
        }

        if (ImGui::BeginPopup("CycleRejected"))
        {
            ImGui::TextDisabled("Re-parent rejected (cycle or invalid).");
            ImGui::EndPopup();
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
