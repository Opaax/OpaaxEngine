#include "SpriteDrawer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Assets/AssetRegistry.h"
#include "ECS/Components/SpriteComponent.h"
#include "Editor/Assets/IAssetTypeActions.h"
#include "Renderer/Texture2D.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    bool SpriteDrawer::HasComponent(World& InWorld, EntityID InEntity) const
    {
        return InWorld.GetComponent<ECS::SpriteComponent>(InEntity) != nullptr;
    }

    void SpriteDrawer::Draw(World& InWorld, EntityID InEntity)
    {
        auto* lS = InWorld.GetComponent<ECS::SpriteComponent>(InEntity);
        if (!lS) { return; }

        const bool bOpen = ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SmallButton("X##RemoveSprite"))
        {
            InWorld.RemoveComponent<ECS::SpriteComponent>(InEntity);
            return;
        }

        if (!bOpen) { return; }

        // OpaaxStringID::ToString returns by value — bind to a local so the
        // backing OpaaxString outlives the ImGui call. CStr() into a temporary
        // dangles past the semicolon.
        const OpaaxString lPath = lS->Texture.IsValid()
                                ? lS->Texture.GetID().ToString()
                                : OpaaxString("None");
        ImGui::LabelText("Texture", "%s", lPath.CStr());

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* lPayload =
                    ImGui::AcceptDragDropPayload(IAssetTypeActions::DragDropPayloadType))
            {
                OPAAX_CORE_ASSERT(lPayload->DataSize == sizeof(Uint32))

                const Uint32        lRawID   = *static_cast<const Uint32*>(lPayload->Data);
                const OpaaxStringID lAssetID(lRawID);

                const auto lHandle = AssetRegistry::Load<Texture2D>(lAssetID);

                if (lHandle.IsValid())
                {
                    lS->Texture = lHandle;
                    OPAAX_CORE_INFO("SpriteDrawer: texture set to '{}'", lAssetID);
                }
                else
                {
                    OPAAX_CORE_WARN("SpriteDrawer: drag & drop failed for '{}'", lAssetID);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::DragFloat2("Size",    &lS->Size.x, 1.f, 0.f, 0.f, "%.2f");
        ImGui::ColorEdit4("Color",   &lS->Color.r);
        ImGui::DragFloat2("UV Min",  &lS->UVMin.x, 0.01f, 0.f, 1.f);
        ImGui::DragFloat2("UV Max",  &lS->UVMax.x, 0.01f, 0.f, 1.f);
        ImGui::Checkbox  ("Visible", &lS->Visible);
    }

    void SpriteDrawer::Add(World& InWorld, EntityID InEntity)
    {
        InWorld.AddComponent<ECS::SpriteComponent>(InEntity);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
