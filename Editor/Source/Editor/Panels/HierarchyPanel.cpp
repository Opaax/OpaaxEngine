#include "Editor/Panels/HierarchyPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"   // the throttled per-map dirty answers
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorSelection.hpp"
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
            TDynArray<EntityID> Entities;
        };
    }

    const char* ToString(EMapAction InAction) noexcept
    {
        switch (InAction)
        {
            case EMapAction::Save:          return "Save Map";
            case EMapAction::SetPersistent: return "Set as Persistent";
            case EMapAction::Remove:        return "Remove from Level";
            case EMapAction::None:          return "None";
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

    void HierarchyPanel::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(260.f, 400.f), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Title.CStr());

        World* lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr)
        {
            ImGui::TextDisabled("No active world.");
            ImGui::End();
            return;
        }

        // Compare by handle: Entity is a value handle, so the selected one is a COPY of the row's entity,
        // never the same object. The handle is the identity.
        const EntityID lSelected = m_Context.Selection.Get().GetHandle();

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
                lGroups.push_back(MapGroup{
                    lMounted.Id, lMounted.AssetRelPath, /*bMounted*/true,
                    /*bPersistent*/!lPersistent.IsEmpty() && lMounted.AssetRelPath == lPersistent,
                    /*bDirty*/m_Context.LevelDocument.IsMapDirtyCached(lMounted.Id), {}});
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
                if (lCandidate.Map == InMeta.OwnerMap) { lGroup = &lCandidate; break; }
            }

            if (lGroup == nullptr)
            {
                lGroups.push_back(MapGroup{InMeta.OwnerMap, {}, /*bMounted*/false, /*bPersistent*/false,
                                           /*bDirty*/false, {}});
                lGroup = &lGroups.back();
            }

            lGroup->Entities.push_back(InId);
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
            OpaaxString lLabel = lGroup.Map.IsValid() ? lGroup.Map.ToString()
                                                      : OpaaxString("(runtime - not saved)");

            // `*` PER MAP, on the map — this is the only place every mounted map is listed, so it
            // is the only place the marker can name which one changed. Read from the throttled
            // cache (**MP5**); a live check here would be a capture per row per frame.
            if (lGroup.bDirty)      { lLabel += " *"; }
            if (lGroup.bPersistent) { lLabel += "  [persistent]"; }

            // The path is the stable ImGui id — a label can repeat, a mounted path cannot.
            ImGui::PushID(lGroup.bMounted ? lGroup.AssetRelPath.CStr() : "runtime");

            const bool lExpanded = ImGui::TreeNodeEx(
                "map",
                lIsFocused ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None,
                "%s", lLabel.CStr());

            DrawMapContextMenu(lGroup.Map, lGroup.AssetRelPath, lGroup.bMounted, lGroup.bPersistent);

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
                if (ImGui::Selectable(lMeta.Name.CStr(), lId == lSelected))
                {
                    m_Context.Selection.Select(lEntity);

                    // Discrete (a click), so no spam — and it is the only observable signal that selection
                    // actually moved, until the Inspector (M2b) renders it.
                    OPAAX_LOG(LogHierarchyPanel, Info, "Hierarchy selected '{}'", lMeta.Name.CStr());
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
            ImGui::PopID();
        }

        ImGui::End();

        // AFTER the walk, and after End(): anything queued above may destroy the very entities the
        // rows just drew.
        RunPendingAction();
    }

    void HierarchyPanel::DrawMapContextMenu(MapId InMapId, const OpaaxString& InAssetRelPath,
                                            bool InMounted, bool InPersistent)
    {
        // The runtime bucket is not a map: there is no file to save, nothing to remove it from, and
        // nothing to make persistent. A menu with four dead entries would be worse than none.
        if (!InMounted) { return; }

        if (!ImGui::BeginPopupContextItem("map_ops")) { return; }

        ImGui::TextDisabled("%s", InAssetRelPath.CStr());
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
            case EMapAction::Save:          MapOps::Save(m_Context, lAction.Map);            break;
            case EMapAction::SetPersistent: MapOps::SetPersistent(m_Context, lAction.Map);   break;
            case EMapAction::Remove:        MapOps::RemoveFromLevel(m_Context, lAction.Map); break;
            case EMapAction::None:                                                           break;
        }
    }
}
