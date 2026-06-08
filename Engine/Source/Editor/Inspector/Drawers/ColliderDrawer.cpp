#include "ColliderDrawer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "Assets/AssetRegistry.h"
#include "ECS/Components/ColliderComponent.h"
#include "Editor/Assets/IAssetTypeActions.h"
#include "Physics/Collision/CollisionProfile.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    bool ColliderDrawer::HasComponent(World& InWorld, EntityID InEntity) const
    {
        return InWorld.GetComponent<ECS::ColliderComponent>(InEntity) != nullptr;
    }

    void ColliderDrawer::Draw(World& InWorld, EntityID InEntity)
    {
        auto* lCol = InWorld.GetComponent<ECS::ColliderComponent>(InEntity);
        if (!lCol) { return; }

        const bool bOpen = ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SmallButton("X##RemoveCollider"))
        {
            InWorld.RemoveComponent<ECS::ColliderComponent>(InEntity);
            return;
        }

        if (!bOpen) { return; }

        static const char* lShapeNames[] = { "Box", "Circle" };
        int lShape = static_cast<int>(lCol->Shape);
        if (ImGui::Combo("Shape", &lShape, lShapeNames, IM_ARRAYSIZE(lShapeNames)))
        {
            lCol->Shape = static_cast<EColliderShape>(lShape);
        }

        static const char* lModeNames[] = { "Solid", "Overlap" };
        int lMode = static_cast<int>(lCol->Mode);
        if (ImGui::Combo("Mode", &lMode, lModeNames, IM_ARRAYSIZE(lModeNames)))
        {
            lCol->Mode = static_cast<EColliderMode>(lMode);
        }

        // Channel (used directly as the filter category when no Profile is assigned).
        const OpaaxString lCurrentChannel = ToStringID(lCol->Channel).ToString();
        if (ImGui::BeginCombo("Channel", lCurrentChannel.CStr()))
        {
            for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
            {
                const bool        bSelected = (i == static_cast<Uint8>(lCol->Channel));
                const OpaaxString lName     = g_CollisionChannelIDs[i].ToString();
                if (ImGui::Selectable(lName.CStr(), bSelected))
                {
                    lCol->Channel = static_cast<ECollisionChannel>(i);
                }
                if (bSelected) { ImGui::SetItemDefaultFocus(); }
            }
            ImGui::EndCombo();
        }

        // Profile asset slot — drag a CollisionProfile from the Asset Browser here.
        const OpaaxString lProfilePath = lCol->Profile.IsValid()
                                       ? lCol->Profile.GetID().ToString()
                                       : OpaaxString("None");
        ImGui::LabelText("Profile", "%s", lProfilePath.CStr());
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* lPayload =
                    ImGui::AcceptDragDropPayload(IAssetTypeActions::DragDropPayloadType))
            {
                OPAAX_CORE_ASSERT(lPayload->DataSize == sizeof(Uint32))
                const Uint32        lRawID = *static_cast<const Uint32*>(lPayload->Data);
                const OpaaxStringID lAssetID(lRawID);

                const auto lHandle = AssetRegistry::Load<CollisionProfile>(lAssetID);
                if (lHandle.IsValid())
                {
                    lCol->Profile = lHandle;
                    OPAAX_CORE_INFO("ColliderDrawer: profile set to '{}'", lAssetID);
                }
                else
                {
                    OPAAX_CORE_WARN("ColliderDrawer: drag & drop failed for '{}'", lAssetID);
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (lCol->Profile.IsValid())
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear##Profile")) { lCol->Profile.Reset(); }
        }

        ImGui::DragFloat2("Offset", &lCol->Offset.x, 1.f, 0.f, 0.f, "%.2f");

        if (lCol->Shape == EColliderShape::Circle)
        {
            ImGui::DragFloat("Radius", &lCol->Radius, 1.f, 0.f, 0.f, "%.2f");
        }
        else
        {
            ImGui::DragFloat2("Size", &lCol->Size.x, 1.f, 0.f, 0.f, "%.2f");
        }

        ImGui::DragFloat("Density",     &lCol->Density,     0.01f, 0.f, 0.f, "%.2f");
        ImGui::DragFloat("Friction",    &lCol->Friction,    0.01f, 0.f, 1.f, "%.2f");
        ImGui::DragFloat("Restitution", &lCol->Restitution, 0.01f, 0.f, 1.f, "%.2f");
    }

    void ColliderDrawer::Add(World& InWorld, EntityID InEntity)
    {
        InWorld.AddComponent<ECS::ColliderComponent>(InEntity);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
