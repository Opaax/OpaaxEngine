#include "Editor/Panels/HierarchyPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"   // the throttled per-map dirty answers
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EntityOps.h"
#include "Editor/Operation/MapOperations.h"

#include "Core/OpaaxTypes.h"
#include "World/Level.h"
#include "World/WorldManager.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        // One map's header and rows. A level holds a handful of maps, so a linear find over these
        // beats a hash map and keeps the headers in MOUNT order instead of a hash order that would
        // reshuffle between frames.
        struct MapGroup
        {
            MapId               Map;            // always VALID when bMounted (MP10); invalid = runtime
            OpaaxString         AssetRelPath;   // empty unless the level mounted it
            bool                bMounted    = false;
            bool                bPersistent = false;
            bool                bDirty      = false;
            bool                bMissing    = false;   // in the manifest, never mounted — no file
            TDynArray<EntityID> Entities;
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
        m_bListLogged = false;

        // A queued verb names a map of the world that just left. Dropping it beats running it
        // against whatever is here now, where the id would either miss or hit the wrong map.
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

        // Rows ask Selection::Contains rather than comparing against one handle — every selected
        // entity highlights, not just the primary the Inspector happens to be drawing.

        // SEEDED FROM THE LEVEL, in mount order. Derived from the entities alone, a map with none
        // of them produced no header at all — so the one panel that lists a level's maps could not
        // show an empty one, which is exactly the map an author has just made and wants to fill.
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

            // THE MANIFEST ENTRIES THAT NEVER MOUNTED — missing, renamed or moved. Derived here
            // rather than asked of the Level, because it is the difference between two lists it
            // already publishes and a getter would allocate one per frame to say the same thing.
            //
            // They get a row for the reason every mounted map does (**MP10**): this is the only
            // place a level's maps are listed, so it is the only place one can be named — and an
            // entry with no row was unreachable, which is why a broken level stayed broken.
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

        // WM2: a Map is a PARTITION of the world's one registry, so bucketing is a FILTER over
        // EntityMeta::OwnerMap and never a second store to keep in sync. An entity whose map is not
        // mounted still gets a group — a bare world (no Level) lists exactly as it did before.
        Uint64 lCount = 0;

        lWorld->Each<EntityMeta>([&](EntityID InId, const EntityMeta& InMeta)
        {
            ++lCount;

            MapGroup* lGroup = nullptr;
            for (MapGroup& lCandidate : lGroups)
            {
                // A MISSING group carries an invalid MapId, and so does a runtime-spawned entity —
                // so without this they match and every runtime entity files itself under a map that
                // does not exist. Skip: a group with no file can own nothing.
                if (lCandidate.bMissing) { continue; }

                if (lCandidate.Map == InMeta.OwnerMap) { lGroup = &lCandidate; break; }
            }

            if (lGroup == nullptr)
            {
                lGroups.emplace_back(MapGroup{InMeta.OwnerMap, {}, /*bMounted*/false, /*bPersistent*/false,
                                           /*bDirty*/false, /*bMissing*/false, {}});
                lGroup = &lGroups.back();
            }

            lGroup->Entities.emplace_back(InId);
        });

        if (lCount == 0 && lGroups.empty())
        {
            ImGui::TextDisabled("World is empty.");
        }
        else if (!m_bListLogged)
        {
            OPAAX_LOG(LogHierarchyPanel, Info, "Hierarchy listing {} entities in {} map(s) from world '{}'",
                      lCount, lGroups.size(), lWorld->GetName().CStr());
            m_bListLogged = true;
        }

        const MapId lFocusedMap = m_Context.MapDocument.GetMapId();

        for (const MapGroup& lGroup : lGroups)
        {
            // NO map is privileged here, and that is the fix: Save Level writes every one of them
            // (MP9), so greying the unfocused maps claimed a difference that does not exist. The
            // focused map only gets the softest hint there is — its group starts open.
            const bool lIsFocused = lGroup.bMounted && lGroup.Map.IsValid() && lGroup.Map == lFocusedMap;

            // Every mounted map names itself, empty or not (**MP10**). The runtime bucket is the
            // one REAL difference that survives: nothing authored those, so no Save can ever write
            // them, and saying so beats the surprise of losing them.
            // A MISSING entry is named by its PATH: it has no MapId to print, and the path is also
            // the only handle anything has on it.
            OpaaxString lLabel = lGroup.bMissing ? lGroup.AssetRelPath
                               : lGroup.Map.IsValid() ? lGroup.Map.ToString()
                                                      : OpaaxString("(runtime - not saved)");

            // `*` PER MAP, on the map — this is the only place every mounted map is listed, so it
            // is the only place the marker can name which one changed. Read from the throttled
            // cache (**MP5**); a live check here would be a capture per row per frame.
            if (lGroup.bDirty)      { lLabel += " *"; }
            if (lGroup.bMissing)    { lLabel += "  [MISSING - file not found]"; }
            if (lGroup.bPersistent) { lLabel += "  [persistent]"; }

            // The path is the stable ImGui id — a label can repeat, a path cannot. Missing entries
            // need it as much as mounted ones: several of them would otherwise share "runtime".
            ImGui::PushID(!lGroup.AssetRelPath.IsEmpty() ? lGroup.AssetRelPath.CStr() : "runtime");

            const bool lExpanded = ImGui::TreeNodeEx(
                "map",
                lIsFocused ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None,
                "%s", lLabel.CStr());

            DrawMapContextMenu(lGroup.Map, lGroup.AssetRelPath, lGroup.bMounted, lGroup.bPersistent,
                               lGroup.bMissing);

            if (!lExpanded)
            {
                ImGui::PopID();
                continue;
            }

            for (const EntityID lId : lGroup.Entities)
            {
                Entity            lEntity{lId, lWorld};
                const EntityMeta& lMeta = lEntity.Get<EntityMeta>();

                // Names are a debug label and may repeat; the handle is what makes each row's ImGui ID unique.
                ImGui::PushID(static_cast<int>(static_cast<Uint32>(lId)));
                if (ImGui::Selectable(lMeta.Name.CStr(), m_Context.Selection.Contains(lEntity)))
                {
                    // Ctrl toggles, a plain click replaces — the convention every editor shares, and
                    // asked of ImGui rather than of the engine's InputManager because an Edit world
                    // leaves the input route closed and Ctrl would read as up forever (IN8).
                    if (ImGui::GetIO().KeyCtrl) { m_Context.Selection.Toggle(lEntity); }
                    else                        { m_Context.Selection.Select(lEntity); }

                    // Discrete (a click), so no spam. The COUNT is what makes a multi-selection
                    // observable at all — one row highlighting looks the same either way.
                    OPAAX_LOG(LogHierarchyPanel, Info, "Hierarchy selected '{}' ({} selected)",
                              lMeta.Name.CStr(), m_Context.Selection.Count());
                }

                DrawEntityContextMenu(lEntity);
                ImGui::PopID();
            }

            ImGui::TreePop();
            ImGui::PopID();
        }

        // AFTER the walk: anything queued above may destroy the very entities the rows just drew.
        RunPendingAction();
    }

    void HierarchyPanel::DrawMapContextMenu(MapId InMapId, const OpaaxString& InAssetRelPath,
                                            bool InMounted, bool InPersistent, bool InMissing)
    {
        // A MISSING entry gets exactly ONE verb. Nothing else applies — there is no file to save, no
        // entities to focus, and making a file that does not exist the persistent map would be
        // worse than the state it is in. This single entry is the whole repair path: before it, the
        // only way out of a broken manifest was hand-editing the .opaaxlevel.
        if (InMissing)
        {
            if (!ImGui::BeginPopupContextItem("missing_map_ops")) { return; }

            ImGui::TextDisabled("%s", InAssetRelPath.CStr());
            ImGui::TextDisabled("This map's file could not be loaded.");
            ImGui::Separator();

            // Disabled for the persistent map, matching RemoveMap's own refusal: dropping it would
            // silently re-point persistence at whatever ended up first.
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

        // The runtime bucket is not a map: there is no file to save, nothing to remove it from, and
        // nothing to make persistent. A menu with four dead entries would be worse than none.
        if (!InMounted) { return; }

        if (!ImGui::BeginPopupContextItem("map_ops")) { return; }

        ImGui::TextDisabled("%s", InAssetRelPath.CStr());
        ImGui::Separator();

        // THE MAP YOU CLICKED IS THE ARGUMENT — the whole reason these verbs live on the header
        // rather than the menu bar (MapOps' own rationale). The Edit menu's Create Entity has to
        // fall back to the focused map because a menu entry names nothing.
        if (ImGui::MenuItem("Create Entity"))
        {
            m_Pending = PendingMapAction{EMapAction::CreateEntity, InMapId, InAssetRelPath};
        }

        ImGui::Separator();

        // NOTHING IS EXECUTED HERE — every entry only RECORDS what was asked for, and Draw runs it
        // after the walk is over. Calling straight through destroyed this frame's entities from the
        // middle of the loop that was about to draw them: Remove from Level unmounts a map, and the
        // rows below this header are handles collected before the click. entt asserted on the first
        // one. A panel's draw pass READS the world; anything that writes it runs after the pass.
        if (ImGui::MenuItem("Save Map"))
        {
            m_Pending = PendingMapAction{EMapAction::Save, InMapId, InAssetRelPath};
        }

        ImGui::Separator();

        // DISABLED RATHER THAN REFUSED. Level::SetPersistentMap on the map that already is one is a
        // no-op and RemoveMap refuses the persistent map outright — both would answer a click with
        // a log line the author never reads. The menu states the rule instead.
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

        // Right-clicking a row that is NOT selected selects it, so "Delete Selected" always means
        // the row under the cursor. Right-clicking one that IS part of a multi-selection leaves the
        // set alone, so the menu acts on all of it — which is what an author expects either way.
        if (!m_Context.Selection.Contains(InEntity))
        {
            m_Context.Selection.Select(InEntity);
        }

        ImGui::TextDisabled("%llu selected", static_cast<unsigned long long>(m_Context.Selection.Count()));
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
        m_Pending = PendingMapAction{};   // cleared FIRST — the action can re-enter nothing, but a
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
            case EMapAction::CreateEntity:   EntityOps::Create(m_Context, lAction.Map, OpaaxString("Entity")); break;
            case EMapAction::DeleteSelected: EntityOps::DestroySelected(m_Context);           break;
            case EMapAction::None:                                                            break;
        }
    }
}
