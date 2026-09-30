#include "Editor/Panels/EntityTreeView.h"

#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Resources/ResourceDragDrop.h"   // prefab drops on rows and headers

#include "Engine/Subsystems/Resources/ResourceManager.h"   // before PrefabResource (completes LoadContext)
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Components/PrefabInstanceComponent.h"   // linked rows are blue
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        Uint32 Bits(const EntityID InId) noexcept { return static_cast<Uint32>(InId); }
    }

    void EntityTreeView::Rebuild(World& InWorld)
    {
        m_Children.clear();
        m_Roots.clear();

        InWorld.Each<EntityMeta>([&](EntityID InId, const EntityMeta&)
        {
            const Entity lParent = EntityHierarchy::GetParent(Entity{ InId, &InWorld });

            if (lParent.IsValid()) { m_Children[Bits(lParent.GetHandle())].emplace_back(InId); }
            else                   { m_Roots.emplace_back(InId); }
        });
    }

    const TDynArray<EntityID>* EntityTreeView::ChildrenOf(const EntityID InEntity) const
    {
        const auto lIt = m_Children.find(Bits(InEntity));
        return lIt != m_Children.end() ? &lIt->second : nullptr;
    }

    void EntityTreeView::DrawNode(World& InWorld, const EntityID InEntity, EditorSelection& InSelection,
                                  const ContextMenuFn& InContextMenu)
    {
        Entity            lEntity{ InEntity, &InWorld };
        const EntityMeta* lMeta = lEntity.IsValid() ? lEntity.TryGet<EntityMeta>() : nullptr;
        if (lMeta == nullptr) { return; }

        const TDynArray<EntityID>* lChildren    = ChildrenOf(InEntity);
        const bool                 lHasChildren = lChildren != nullptr && !lChildren->empty();

        ImGuiTreeNodeFlags lFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                  | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!lHasChildren)                 { lFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; }
        if (InSelection.Contains(lEntity)) { lFlags |= ImGuiTreeNodeFlags_Selected; }

        // Names can repeat; the handle makes each row's ImGui ID unique.
        ImGui::PushID(static_cast<int>(Bits(InEntity)));

        // A prefab instance is shown in blue (like Unity), so it can be told apart from plain entities.
        const PrefabInstanceComponent* lMarker = lEntity.TryGet<PrefabInstanceComponent>();
        const bool lLinked = lMarker != nullptr && lMarker->IsLinked();
        if (lLinked) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.f, 1.f)); }

        const bool lOpen = ImGui::TreeNodeEx("node", lFlags, "%s", lMeta->Name.CStr());

        if (lLinked)
        {
            ImGui::PopStyleColor();
            ImGui::SetItemTooltip("Instance of %s", lMarker->Prefab.Path.CStr());
        }

        // The arrow toggles, the rest selects. Ctrl toggles, a plain click replaces. Asked of ImGui (with
        // an Edit world the engine never sees Ctrl).
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            if (ImGui::GetIO().KeyCtrl) { InSelection.Toggle(lEntity); }
            else                        { InSelection.Select(lEntity); }

            // A click (no spam). The count shows a multi-selection.
            OPAAX_LOG(LogEntityTreeView, Info, "Hierarchy selected '{}' ({} selected)",
                      lMeta->Name.CStr(), InSelection.Count());
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload(k_Payload, &InEntity, sizeof(EntityID));
            ImGui::Text("%s", lMeta->Name.CStr());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* lPayload = ImGui::AcceptDragDropPayload(k_Payload))
            {
                m_Drop     = EntityTreeDrop{ *static_cast<const EntityID*>(lPayload->Data), InEntity, MapId{} };
                m_bHasDrop = true;
            }
            ImGui::EndDragDropTarget();
        }

        // A prefab dropped on a row: an instance as this entity's child.
        if (OpaaxString lPrefab; AcceptResourceDragPayload(ResourceTypeID::Get<PrefabResource>(), lPrefab))
        {
            m_PrefabDrop     = EntityTreePrefabDrop{ Move(lPrefab), InEntity, MapId{} };
            m_bHasPrefabDrop = true;
        }

        InContextMenu(lEntity);

        if (lOpen && lHasChildren)
        {
            for (const EntityID lChild : *lChildren)
            {
                DrawNode(InWorld, lChild, InSelection, InContextMenu);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void EntityTreeView::AcceptRootDrop(const MapId InMap)
    {
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* lPayload = ImGui::AcceptDragDropPayload(k_Payload))
            {
                m_Drop     = EntityTreeDrop{ *static_cast<const EntityID*>(lPayload->Data), ENTITY_NONE, InMap };
                m_bHasDrop = true;
            }
            ImGui::EndDragDropTarget();
        }

        // A prefab dropped on a map header: an instance in that map, at its authored position.
        if (OpaaxString lPrefab; AcceptResourceDragPayload(ResourceTypeID::Get<PrefabResource>(), lPrefab))
        {
            m_PrefabDrop     = EntityTreePrefabDrop{ Move(lPrefab), ENTITY_NONE, InMap };
            m_bHasPrefabDrop = true;
        }
    }

    void EntityTreeView::DrawUnparentStrip(const MapId InMap)
    {
        // Only while one of our payloads is dragged (a dragged texture shows nothing).
        const ImGuiPayload* lInFlight = ImGui::GetDragDropPayload();
        if (lInFlight == nullptr || !lInFlight->IsDataType(k_Payload)) { return; }

        ImGui::Selectable("Drop here to unparent", false, ImGuiSelectableFlags_None, ImVec2(0.f, 0.f));
        AcceptRootDrop(InMap);
    }

    bool EntityTreeView::TakeDrop(EntityTreeDrop& OutDrop)
    {
        if (!m_bHasDrop) { return false; }

        OutDrop    = m_Drop;
        m_Drop     = EntityTreeDrop{};
        m_bHasDrop = false;
        return true;
    }

    bool EntityTreeView::TakePrefabDrop(EntityTreePrefabDrop& OutDrop)
    {
        if (!m_bHasPrefabDrop) { return false; }

        OutDrop          = Move(m_PrefabDrop);
        m_PrefabDrop     = EntityTreePrefabDrop{};
        m_bHasPrefabDrop = false;
        return true;
    }
}
