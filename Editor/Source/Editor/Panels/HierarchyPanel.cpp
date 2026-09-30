#include "Editor/Panels/HierarchyPanel.h"

#include "Editor/EditorContext.h"
#include "Application/Services/IPaths.h"
#include "Editor/EditorLevelDocument.h"   // per-map dirty state
#include "Editor/EditorMapDocument.h"
#include "Editor/Commands/EditorNativeCommands.h"       // MapIdParams
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EntityOps.h"
#include "Editor/Operation/MapOperations.h"

#include "Core/OpaaxTypes.h"
#include "World/Level.h"
#include "World/WorldManager.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"   // Detach needs a parent
#include "World/Entity/EntityMeta.h"
#include "World/Components/PrefabInstanceComponent.h"   // the revert entries need the link

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        // One map's header and rows. Kept in a list (few maps) so headers stay in mount order.
        struct MapGroup
        {
            MapId               Map;            // valid when bMounted; invalid = runtime
            OpaaxString         AssetRelPath;   // empty unless mounted by the level
            bool                bMounted    = false;
            bool                bPersistent = false;
            bool                bDirty      = false;
            bool                bMissing    = false;   // in the manifest but never mounted
            TDynArray<EntityID> Roots;      // roots only; children are drawn under their parent
        };
    }

    const char* ToString(EMapAction InAction) noexcept
    {
        switch (InAction)
        {
            case EMapAction::Save:           return "Save Map";
            case EMapAction::SetPersistent:  return "Set as Persistent";
            case EMapAction::Remove:         return "Remove from Level";
            case EMapAction::RemoveMissing:  return "Remove Missing from Level";
            case EMapAction::CreateEntity:   return "Create Entity";
            case EMapAction::DeleteSelected: return "Delete Selected";
            case EMapAction::CreatePrefab:   return "Create Prefab from Selection";
            case EMapAction::RevertPrefab:   return "Revert to Prefab";
            case EMapAction::RevertPrefabAll: return "Revert Instance to Prefab";
            case EMapAction::Detach:         return "Detach from Parent";
            case EMapAction::None:           return "None";
        }

        return "None";
    }

    HierarchyPanel::HierarchyPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    HierarchyPanel::~HierarchyPanel() = default;

    void HierarchyPanel::OnActiveWorldChanged(World* /*InOld*/, World* /*InNew*/)
    {
        // A queued action names a map of the old world: drop it.
        m_Pending = PendingMapAction{};
    }

    void HierarchyPanel::DrawContents()
    {
        World* lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr)
        {
            ImGui::TextDisabled("No active world.");
            return;
        }

        // Rows use Selection::Contains, so every selected entity is highlighted.

        // Headers come from the Level, in mount order, so an empty map still has a header.
        TDynArray<MapGroup> lGroups;
        Level* const        lLevel = MapOps::ActiveLevel(m_Context);

        if (lLevel != nullptr)
        {
            const OpaaxString& lPersistent = lLevel->GetData().PersistentMap();

            for (const Level::MountedMap& lMounted : lLevel->GetMountedMaps())
            {
                lGroups.emplace_back(MapGroup{
                    lMounted.Id, lMounted.AssetRelPath, /*bMounted*/true,
                    /*bPersistent*/!lPersistent.IsEmpty() && lMounted.AssetRelPath == lPersistent,
                    /*bDirty*/m_Context.LevelDocument.IsMapDirtyCached(lMounted.Id),
                    /*bMissing*/false, {}});
            }

            // Manifest entries that never mounted (missing, renamed or moved) get a row too: this is the only
            // place they can be removed from.
            for (const OpaaxString& lPath : lLevel->GetData().Maps)
            {
                bool lListed = false;
                for (const MapGroup& lGroup : lGroups)
                {
                    if (lGroup.AssetRelPath == lPath) { lListed = true; break; }
                }

                if (lListed) { continue; }

                lGroups.emplace_back(MapGroup{
                    MapId{}, lPath, /*bMounted*/false, /*bPersistent*/!lPersistent.IsEmpty() && lPath == lPersistent,
                    /*bDirty*/false, /*bMissing*/true, {}});
            }
        }

        // Entities are bucketed by EntityMeta::OwnerMap. Unmounted maps still get a group. Only roots
        // are bucketed; children are drawn under their parent.
        const Uint64 lCount = lWorld->GetEntityCount();

        m_Tree.Rebuild(*lWorld);

        for (const EntityID lId : m_Tree.Roots())
        {
            const EntityMeta& lMeta = Entity{ lId, lWorld }.Get<EntityMeta>();

            MapGroup* lGroup = nullptr;
            for (MapGroup& lCandidate : lGroups)
            {
                // A missing group has an invalid MapId, like runtime entities: skip it so they do not match.
                if (lCandidate.bMissing) { continue; }

                if (lCandidate.Map == lMeta.OwnerMap) { lGroup = &lCandidate; break; }
            }

            if (lGroup == nullptr)
            {
                lGroups.emplace_back(MapGroup{lMeta.OwnerMap, {}, /*bMounted*/false, /*bPersistent*/false,
                                           /*bDirty*/false, /*bMissing*/false, {}});
                lGroup = &lGroups.back();
            }

            lGroup->Roots.emplace_back(lId);
        }

        if (lCount == 0 && lGroups.empty())
        {
            ImGui::TextDisabled("World is empty.");
        }

        const MapId lFocusedMap = m_Context.MapDocument.GetMapId();

        for (const MapGroup& lGroup : lGroups)
        {
            // No map is privileged (Save Level writes them all); the focused map's group just starts open.
            const bool lIsFocused = lGroup.bMounted && lGroup.Map.IsValid() && lGroup.Map == lFocusedMap;

            // Every mounted map shows its name. The runtime group says its entities are not saved.
            // A missing entry is shown by its path.
            OpaaxString lLabel = lGroup.bMissing ? lGroup.AssetRelPath
                               : lGroup.Map.IsValid() ? lGroup.Map.ToString()
                                                      : OpaaxString("(runtime - not saved)");

            // A * per map, from the throttled cache (a live check would cost a capture per row).
            if (lGroup.bDirty)      { lLabel += " *"; }
            if (lGroup.bMissing)    { lLabel += "  [MISSING - file not found]"; }
            if (lGroup.bPersistent) { lLabel += "  [persistent]"; }

            // The path is the ImGui id (labels can repeat).
            ImGui::PushID(!lGroup.AssetRelPath.IsEmpty() ? lGroup.AssetRelPath.CStr() : "runtime");

            const bool lExpanded = ImGui::TreeNodeEx(
                "map",
                lIsFocused ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None,
                "%s", lLabel.CStr());

            // A drop on the header: to the root of this map. The runtime group only detaches.
            if (!lGroup.bMissing) { m_Tree.AcceptRootDrop(lGroup.Map); }

            DrawMapContextMenu(lGroup.Map, lGroup.AssetRelPath, lGroup.bMounted, lGroup.bPersistent,
                               lGroup.bMissing);

            if (!lExpanded)
            {
                ImGui::PopID();
                continue;
            }

            // The rows: roots with their children, drag and drop stored for later.
            for (const EntityID lId : lGroup.Roots)
            {
                m_Tree.DrawNode(*lWorld, lId, m_Context.Selection,
                                [this](Entity InEntity) { DrawEntityContextMenu(InEntity); });
            }

            ImGui::TreePop();
            ImGui::PopID();
        }

        // After the walk: queued actions may destroy the entities the rows just drew.
        RunPendingAction();

        // Then the stored drop: one command, one undo step.
        EntityTreeDrop lDrop;
        if (m_Tree.TakeDrop(lDrop))
        {
            EntityOps::Reparent(m_Context, EUndoWorld::Active, lDrop.Child, lDrop.Parent, lDrop.ToMap);
        }

        // A prefab from the browser: on a header, into that map at its authored position; on a row,
        // into that row's map as its child.
        EntityTreePrefabDrop lPrefabDrop;
        if (m_Tree.TakePrefabDrop(lPrefabDrop))
        {
            MapId lMap = lPrefabDrop.ToMap;
            if (lPrefabDrop.OnEntity != ENTITY_NONE && lWorld->IsValid(lPrefabDrop.OnEntity))
            {
                lMap = Entity{ lPrefabDrop.OnEntity, lWorld }.Get<EntityMeta>().OwnerMap;
            }

            // InstantiatePrefab refuses an invalid map itself (and says so).
            EntityOps::InstantiatePrefab(m_Context, m_Context.Paths.AssetToAbsolute(lPrefabDrop.AssetPath),
                                         lMap, nullptr, lPrefabDrop.OnEntity);
        }

    }

    void HierarchyPanel::DrawMapContextMenu(MapId InMapId, const OpaaxString& InAssetRelPath,
                                            bool InMounted, bool InPersistent, bool InMissing)
    {
        // A missing entry has one action: remove it from the level.
        if (InMissing)
        {
            if (!ImGui::BeginPopupContextItem("missing_map_ops")) { return; }

            ImGui::TextDisabled("%s", InAssetRelPath.CStr());
            ImGui::TextDisabled("This map's file could not be loaded.");
            ImGui::Separator();

            // Disabled for the persistent map (RemoveMap refuses it).
            if (ImGui::MenuItem("Remove from Level", nullptr, false, !InPersistent))
            {
                m_Pending = PendingMapAction{EMapAction::RemoveMissing, InMapId, InAssetRelPath};
            }

            if (InPersistent)
            {
                ImGui::TextDisabled("It is the persistent map — set\nanother one persistent first.");
            }

            ImGui::EndPopup();
            return;
        }

        // The runtime group is not a map: no menu.
        if (!InMounted) { return; }

        if (!ImGui::BeginPopupContextItem("map_ops")) { return; }

        ImGui::TextDisabled("%s", InAssetRelPath.CStr());
        ImGui::Separator();

        // The clicked map is the argument (the Edit menu's Create Entity uses the focused map).
        if (ImGui::MenuItem("Create Entity"))
        {
            m_Pending = PendingMapAction{EMapAction::CreateEntity, InMapId, InAssetRelPath};
        }

        ImGui::Separator();

        // Nothing runs here: entries only record the request, and Draw runs it after the walk
        // (running it now could destroy entities the rows below are about to draw).
        if (ImGui::MenuItem("Save Map"))
        {
            m_Pending = PendingMapAction{EMapAction::Save, InMapId, InAssetRelPath};
        }

        ImGui::Separator();

        // Disabled instead of refused: the menu shows what applies.
        if (ImGui::MenuItem("Set as Persistent", nullptr, false, !InPersistent))
        {
            m_Pending = PendingMapAction{EMapAction::SetPersistent, InMapId, InAssetRelPath};
        }

        if (ImGui::MenuItem("Remove from Level", nullptr, false, !InPersistent))
        {
            m_Pending = PendingMapAction{EMapAction::Remove, InMapId, InAssetRelPath};
        }

        if (InPersistent)
        {
            ImGui::Separator();
            ImGui::TextDisabled("The persistent map is the backdrop\nevery other map composes on.");
        }

        ImGui::EndPopup();
    }

    void HierarchyPanel::DrawEntityContextMenu(Entity InEntity)
    {
        if (!ImGui::BeginPopupContextItem("entity_ops")) { return; }

        // Right-clicking an unselected row selects it; right-clicking inside a multi-selection keeps it.
        if (!m_Context.Selection.Contains(InEntity))
        {
            m_Context.Selection.Select(InEntity);
        }

        ImGui::TextDisabled("%llu selected", static_cast<unsigned long long>(m_Context.Selection.Count()));
        ImGui::Separator();

        // Queued: it destroys the selection and creates an instance, and opens a dialog.
        if (ImGui::MenuItem("Create Prefab from Selection..."))
        {
            m_Pending = PendingMapAction{EMapAction::CreatePrefab, MapId{}, {}};
        }

        // Disabled when nothing selected comes from a prefab. Same for Detach (a root has no parent).
        bool lHasLink   = false;
        bool lHasParent = false;
        if (World* const lWorld = m_Context.Worlds.GetActiveWorld(); lWorld != nullptr)
        {
            for (const EntityID lId : m_Context.Selection.Ids())
            {
                Entity lCandidate{ lId, lWorld };
                if (!lCandidate.IsValid()) { continue; }

                lHasLink   = lHasLink   || lCandidate.Has<PrefabInstanceComponent>();
                lHasParent = lHasParent || EntityHierarchy::GetParent(lCandidate).IsValid();
            }
        }

        if (ImGui::MenuItem("Detach from Parent", nullptr, false, lHasParent))
        {
            m_Pending = PendingMapAction{EMapAction::Detach, MapId{}, {}};
        }

        ImGui::Separator();

        // A disabled entry says why.
        const auto lWhyDisabled = [lHasLink]()
        {
            if (!lHasLink && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Nothing selected is a prefab instance");
            }
        };

        if (ImGui::MenuItem("Revert to Prefab", nullptr, false, lHasLink))
        {
            m_Pending = PendingMapAction{EMapAction::RevertPrefab, MapId{}, {}};
        }
        lWhyDisabled();

        if (ImGui::MenuItem("Revert Instance to Prefab", nullptr, false, lHasLink))
        {
            m_Pending = PendingMapAction{EMapAction::RevertPrefabAll, MapId{}, {}};
        }
        lWhyDisabled();

        ImGui::Separator();

        if (ImGui::MenuItem("Delete"))
        {
            m_Pending = PendingMapAction{EMapAction::DeleteSelected, MapId{}, {}};
        }

        ImGui::EndPopup();
    }

    void HierarchyPanel::RunPendingAction()
    {
        const PendingMapAction lAction = m_Pending;
        m_Pending = PendingMapAction{};   // cleared first, so a failed action does not run again next frame
                                          // failed one must not run again next frame either

        if (lAction.Action == EMapAction::None) { return; }

        OPAAX_LOG(LogHierarchyPanel, Info, "Map menu: '{}' on '{}'",
                  ToString(lAction.Action), lAction.AssetRelPath.CStr());

        switch (lAction.Action)
        {
            case EMapAction::Save:           MapOps::Save(m_Context, lAction.Map);            break;
            case EMapAction::SetPersistent:  MapOps::SetPersistent(m_Context, lAction.Map);   break;
            case EMapAction::Remove:         MapOps::RemoveFromLevel(m_Context, lAction.Map); break;
            case EMapAction::RemoveMissing:  MapOps::RemoveMissingFromLevel(m_Context, lAction.AssetRelPath); break;
            // By tag, like the Edit menu and the keys: the dispatch records the undo step. The payload
            // carries the clicked map.
            case EMapAction::CreateEntity:
                m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_CREATE_ENTITY, m_Context,
                                                        MapIdParams{ lAction.Map });
                break;
            case EMapAction::DeleteSelected:
                m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_DELETE_ENTITY, m_Context);
                break;
            // Directly: it records its own undo step.
            case EMapAction::Detach:
                EntityOps::DetachSelected(m_Context, EUndoWorld::Active);
                break;
            // No payload: the subject is the selection.
            case EMapAction::CreatePrefab:
                m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_CREATE_PREFAB_FROM_SELECTION,
                                                        m_Context);
                break;
            // One command; the scope is in the payload.
            case EMapAction::RevertPrefab:
                m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_REVERT_TO_PREFAB, m_Context,
                                                        PrefabRevertParams{ /*bWholeInstance*/false });
                break;
            case EMapAction::RevertPrefabAll:
                m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_REVERT_TO_PREFAB, m_Context,
                                                        PrefabRevertParams{ /*bWholeInstance*/true });
                break;
            case EMapAction::None:                                                            break;
        }
    }
}
