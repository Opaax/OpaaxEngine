#include "HierarchyPanel.h"

#if OPAAX_WITH_EDITOR

#include <cstdio>
#include <unordered_map>
#include <imgui.h>
#include <entt/entt.hpp>

#include "Core/Log/OpaaxLog.h"
#include "ECS/Components/ParentComponent.h"
#include "ECS/Components/SceneIDComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Hierarchy.h"
#include "Editor/EditorEventBus.h"
#include "Editor/Events/EditorEvents.h"
#include "World/World.h"

namespace Opaax::Editor
{
    namespace
    {
        constexpr const char* kEntityDragPayload = "OPAAX_ENTITY";

        // Render one entity row + recurse into its children. Returns the entity to delete
        // when the user picks "Delete Entity" from the context menu (ENTITY_NONE = none).
        EntityID DrawNode(World& InWorld,
                          EntityID InEntity,
                          const UnorderedMap<EntityID, TDynArray<EntityID>>& InChildren,
                          EntityID& InOutSelected)
        {
            EntityID lToDelete = ENTITY_NONE;

            // Materialize the tag into a stable buffer.
            // OpaaxStringID::ToString() returns an OpaaxString *by value* — its CStr()
            // points into a temporary that dies at the end of the full expression, so
            // storing that pointer in a local would dangle by the next ImGui call.
            char lName[128];
            if (const auto* lTag = InWorld.GetComponent<ECS::TagComponent>(InEntity))
            {
                std::snprintf(lName, sizeof(lName), "%s", lTag->Tag.ToString().CStr());
            }
            else
            {
                std::snprintf(lName, sizeof(lName), "<no tag>");
            }

            const auto lIt = InChildren.find(InEntity);
            const bool bHasChildren = (lIt != InChildren.end()) && !lIt->second.empty();

            ImGuiTreeNodeFlags lFlags =
                ImGuiTreeNodeFlags_OpenOnArrow         |
                ImGuiTreeNodeFlags_OpenOnDoubleClick   |
                ImGuiTreeNodeFlags_SpanAvailWidth;

            if (!bHasChildren) { lFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; }
            if (InEntity == InOutSelected) { lFlags |= ImGuiTreeNodeFlags_Selected; }

            char lLabel[160];
            std::snprintf(lLabel, sizeof(lLabel), "%s##%u",
                lName, static_cast<Uint32>(InEntity));

            const bool bOpen = ImGui::TreeNodeEx(lLabel, lFlags);

            // Selection on click — but only when not toggling the arrow.
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                InOutSelected = InEntity;
            }

            // Drag source: this entity.
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload(kEntityDragPayload, &InEntity, sizeof(EntityID));
                ImGui::Text("%s", lName);
                ImGui::EndDragDropSource();
            }

            // Drop target: re-parent the dragged entity onto this one.
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* lPayload = ImGui::AcceptDragDropPayload(kEntityDragPayload))
                {
                    OPAAX_CORE_ASSERT(lPayload->DataSize == sizeof(EntityID))
                    const EntityID lDragged = *static_cast<const EntityID*>(lPayload->Data);
                    if (lDragged != InEntity)
                    {
                        if (!ECS::Hierarchy::SetParent(InWorld, lDragged, InEntity))
                        {
                            OPAAX_CORE_TRACE("HierarchyPanel: drag re-parent rejected (cycle).");
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Right-click context menu.
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Delete Entity"))
                {
                    lToDelete = InEntity;
                }
                ImGui::EndPopup();
            }

            // Recurse into children.
            if (bOpen && bHasChildren)
            {
                for (EntityID lChild : lIt->second)
                {
                    const EntityID lChildDelete = DrawNode(InWorld, lChild, InChildren, InOutSelected);
                    if (lChildDelete != ENTITY_NONE && lToDelete == ENTITY_NONE)
                    {
                        lToDelete = lChildDelete;
                    }
                }
                ImGui::TreePop();
            }

            return lToDelete;
        }
    }

    void HierarchyPanel::OnSubscribe(EditorEventBus& InBus)
    {
        m_Bus = &InBus;

        m_NewSceneToken = InBus.Subscribe<OnNewSceneEvent>(
            [this](const OnNewSceneEvent&)
            {
                OPAAX_CORE_INFO("Hierarchy Panel - OnNewSceneEvent received, clearing selection");
                SetSelection(ENTITY_NONE);
            });
    }

    void HierarchyPanel::SetSelectedEntity(EntityID InEntity) { SetSelection(InEntity); }
    void HierarchyPanel::ClearSelection()                     { SetSelection(ENTITY_NONE); }

    void HierarchyPanel::SetSelection(EntityID InEntity)
    {
        if (InEntity == m_SelectedEntity) { return; } // idempotent — no spurious publishes

        m_SelectedEntity = InEntity;

        OPAAX_CORE_INFO("Hierarchy Panel - New Entity selected: {}", "Implement Entity::GetName");

        if (m_Bus != nullptr)
        {
            OnEntitySelectedEvent lEvent{m_SelectedEntity};
            m_Bus->Publish(lEvent);
        }
    }

    void HierarchyPanel::Draw(SceneManager& InSceneMgr, World& InWorld)
    {
        ImGui::Begin("Hierarchy");

        // Self-fetch active scene per frame — never cache Scene* across MainMenuBar.Draw
        // or any other panel call. Stack top can change mid-DrawPanels (PIE Stop,
        // New Scene, Open Scene) so the freshest read is the only safe one.
        Scene* lScene = InSceneMgr.GetActiveScene();
        if (!lScene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        World& lWorld    = InWorld;
        auto&  lRegistry = lWorld.GetRegistry();

        // Scene name as header
        ImGui::SeparatorText(lScene->GetName().CStr());

        // Entity count
        ImGui::TextDisabled("%u entities", lWorld.GetEntityCount());
        ImGui::Separator();

        // Build a parent->children index for this frame. Filter entities to the
        // active scene's SceneID + persistents (SceneID == 0) so opening a
        // different scene doesn't show the previous scene's entities, and so
        // persistent state stays visible across transitions.
        const Uint32 lActiveSceneID = lScene->GetSceneID();
        UnorderedMap<EntityID, TDynArray<EntityID>> lChildren;
        TDynArray<EntityID> lRoots;
        {
            auto lView = lRegistry.view<const ECS::TagComponent, const ECS::SceneIDComponent>();
            for (auto lEnt : lView)
            {
                const Uint32 lSID = lView.get<const ECS::SceneIDComponent>(lEnt).SceneID;
                if (lSID != lActiveSceneID && lSID != World::PersistentSceneID) { continue; }

                const auto* lP = lWorld.GetComponent<ECS::ParentComponent>(lEnt);
                if (lP && lP->Parent != ENTITY_NONE && lRegistry.valid(lP->Parent))
                {
                    lChildren[lP->Parent].push_back(lEnt);
                }
                else
                {
                    lRoots.push_back(lEnt);
                }
            }
        }

        // Render the tree. Collect a single delete request — the registry mutation
        // happens after iteration to avoid invalidating views mid-walk.
        // DrawNode mutates its EntityID& parameter in-place on click; route through
        // a local so SetSelection (which publishes) is the single sink.
        EntityID lWorkingSelection = m_SelectedEntity;
        EntityID lToDelete = ENTITY_NONE;
        for (EntityID lRoot : lRoots)
        {
            const EntityID lDel = DrawNode(lWorld, lRoot, lChildren, lWorkingSelection);
            if (lDel != ENTITY_NONE && lToDelete == ENTITY_NONE)
            {
                lToDelete = lDel;
            }
        }

        SetSelection(lWorkingSelection);

        // Drop-to-root zone: the row IS the drop target AND the visible label, so the
        // user can never get confused about which surface receives the drop.
        // Selectable styles match other tree rows; click does nothing on purpose.
        ImGui::Selectable("Drop here to detach to root",
                          /*selected*/ false,
                          ImGuiSelectableFlags_None,
                          ImVec2(0.f, 22.f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* lPayload = ImGui::AcceptDragDropPayload(kEntityDragPayload))
            {
                OPAAX_CORE_ASSERT(lPayload->DataSize == sizeof(EntityID))
                const EntityID lDragged = *static_cast<const EntityID*>(lPayload->Data);
                ECS::Hierarchy::ClearParent(lWorld, lDragged);
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::Separator();

        // Process deferred destroy.
        if (lToDelete != ENTITY_NONE)
        {
            const bool bWasSelected =
                (m_SelectedEntity == lToDelete) ||
                ECS::Hierarchy::IsDescendantOf(lWorld, m_SelectedEntity, lToDelete);

            lWorld.DestroyEntity(lToDelete, /*bDestroyChildren*/ true);

            if (bWasSelected) { ClearSelection(); }
        }

        // Create entity button
        if (ImGui::Button("+ Add Entity", ImVec2(-1.f, 0.f)))
        {
            const EntityID lNew = lWorld.CreateEntity("NewEntity");
            SetSelection(lNew);
        }

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
