#include "Editor/Panels/EntityTreeView.h"

#include "Editor/Operation/EditorSelection.hpp"

#include "World/Components/PrefabInstanceComponent.h"   // a linked row reads blue
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

        // Names are a debug label and may repeat; the handle is what makes each row's ImGui ID unique.
        ImGui::PushID(static_cast<int>(Bits(InEntity)));

        // A PREFAB INSTANCE reads blue, Unity's convention. Without it an author cannot tell an
        // instance from the plain entities an Undo of Create Prefab puts back — and then wonders
        // why a prefab save reaches nothing and Revert is grey.
        const PrefabInstanceComponent* lMarker = lEntity.TryGet<PrefabInstanceComponent>();
        const bool lLinked = lMarker != nullptr && lMarker->IsLinked();
        if (lLinked) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.f, 1.f)); }

        const bool lOpen = ImGui::TreeNodeEx("node", lFlags, "%s", lMeta->Name.CStr());

        if (lLinked)
        {
            ImGui::PopStyleColor();
            ImGui::SetItemTooltip("Instance of %s", lMarker->Prefab.Path.CStr());
        }

        // The arrow toggles, the rest of the row selects. Ctrl toggles, a plain click replaces —
        // asked of ImGui rather than the InputManager because an Edit world leaves the input route
        // closed and Ctrl would read as up forever (IN8).
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            if (ImGui::GetIO().KeyCtrl) { InSelection.Toggle(lEntity); }
            else                        { InSelection.Select(lEntity); }

            // Discrete (a click), so no spam. The COUNT is what makes a multi-selection observable.
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
        if (!ImGui::BeginDragDropTarget()) { return; }

        if (const ImGuiPayload* lPayload = ImGui::AcceptDragDropPayload(k_Payload))
        {
            m_Drop     = EntityTreeDrop{ *static_cast<const EntityID*>(lPayload->Data), ENTITY_NONE, InMap };
            m_bHasDrop = true;
        }
        ImGui::EndDragDropTarget();
    }

    void EntityTreeView::DrawUnparentStrip(const MapId InMap)
    {
        // Only while one of OUR payloads is in flight — the strip is an affordance for a drag, not
        // furniture. Checked by type, so a texture dragged out of the browser shows nothing here.
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
}
