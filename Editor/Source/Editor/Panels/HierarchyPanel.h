#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "World/Entity/EntityTypes.h"   // MapId — the context menu's target

namespace Opaax
{
    class Entity;

    OPAAX_LOG_CATEGORY(HierarchyPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * Which per-map verb a header's context menu asked for.
     *
     * The menu RECORDS one of these instead of calling straight through: Remove from Level destroys
     * entities, and the click arrives in the middle of the loop that is about to draw them.
     */
    enum class EMapAction
    {
        None,
        Save,
        SetPersistent,
        Remove,
        RemoveMissing,  // a manifest entry whose file never mounted — named by PATH, it has no id
        CreateEntity,   // ② — into the map whose header was clicked
        DeleteSelected  // ② — the row's own menu; the row is selected first, so it needs no target
    };

    /** **I11** — an enum gets a free ToString, found by ADL, declared with the enum. */
    const char* ToString(EMapAction InAction) noexcept;

    // =============================================================================
    // HierarchyPanel — the dockable "Hierarchy" panel: one selectable row per entity in the active
    //   world, GROUPED BY THE MAP THAT AUTHORED IT, and the editor's single WRITER of
    //   EditorSelection (the Inspector, M2b, is the reader).
    //
    //   Enumerates through World::Each<EntityMeta>, which is the all-entities view by construction —
    //   World::CreateEntity always emplaces EntityMeta — so listing entities needs no World/ECS API.
    //   The groups are a filter over EntityMeta::OwnerMap (WM2), which is also why the invalid id
    //   gets its own "(runtime - not saved)" header rather than being hidden: it means "no map
    //   authored this", and therefore that no Save will ever write it.
    //
    //   THE HEADERS COME FROM THE LEVEL, NOT FROM THE ENTITIES. Groups are seeded from
    //   Level::GetMountedMaps() in mount order and the entities are bucketed into them, so a map
    //   that is in the world with NOTHING IN IT still has a header. Derived purely from entities it
    //   had none — an empty map was invisible in the one panel that lists maps.
    //
    //   IT IS ALSO WHERE A MAP IS PICKED, AND WHERE ITS `*` LIVES. Per-map verbs (save, set
    //   persistent, remove) hang off the header's context menu, and the unsaved marker sits on the
    //   map it belongs to — this is the only place every mounted map is listed, so it is the only
    //   place either can name which map they mean. The menu bar's "Remove Open Map" / "Set Open Map
    //   Persistent" acted on whatever was focused, which is not a way to choose one map out of
    //   several (WM1a). Bodies are MapOps', shared with the File menu so the two cannot drift; the
    //   `*` is read from EditorLevelDocument's THROTTLED cache, never re-derived per row (MP5).
    //
    //   It is a NATIVE editor panel with no privileges: EditorService registers it into the same
    //   PanelRegistry a game module registers into, and it is constructed by the same loop. That
    //   symmetry is the thing M2a exists to prove, so keep it registered — never hand-built.
    // =============================================================================
    class HierarchyPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Hierarchy);
        
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit HierarchyPanel(EditorContext& InContext);
        ~HierarchyPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        HierarchyPanel(const HierarchyPanel&)            = delete;
        HierarchyPanel& operator=(const HierarchyPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * The right-clicked map's verbs, from the ONE place a level's maps are listed. The bodies
         * are MapOps', shared with the File menu, so the two call sites cannot drift.
         *
         * Entries are DISABLED rather than left to be refused: Level::RemoveMap turns down the
         * persistent map and SetPersistentMap on it is a no-op, and both would answer a click with
         * a log line nobody reads.
         *
         * @param InMapId       Always valid when InMounted: a map names itself (**MP10**).
         * @param InMounted     False for the runtime bucket, which is not a map and gets no menu.
         * @param InPersistent  The level's backdrop map (**WM1a**).
         * @param InMissing     A manifest entry whose file never loaded. Gets ONE verb — remove —
         *                      because nothing else applies to a map with no file, and because that
         *                      entry is otherwise unreachable from the editor entirely.
         */
        void DrawMapContextMenu(MapId InMapId, const OpaaxString& InAssetRelPath,
                                bool InMounted, bool InPersistent, bool InMissing);

        /**
         * The right-clicked ENTITY's verbs. Right-clicking a row SELECTS it first, so Delete needs
         * no target of its own — what you right-clicked is what is selected, which is also what
         * keeps this consistent with the Delete key and the Edit menu.
         *
         * Queued like every other verb here: destroying entities mid-walk invalidates the handles
         * the rows below were collected from.
         */
        void DrawEntityContextMenu(Entity InEntity);

        /**
         * Run whatever the context menu queued, AFTER the draw pass — the panel's draw is a READ of
         * the world, and these write it.
         *
         * Not caution: calling Remove from Level inline destroyed the entities whose handles the
         * rows under that same header were collected from, and entt asserted on the first one.
         */
        void RunPendingAction();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** No resource to acquire — the panel reads the world through the context. */
        void            Startup()               override {}

        /** Nothing the world's render depends on — the row list is built in Draw. */
        void            OnPreRender()           override {}

        /**
         * One collapsible header per MOUNTED MAP, one ImGui::Selectable per entity under it,
         * highlighted when it matches the current selection; a click writes the selection, a
         * right-click on the header opens that map's verbs. Every map draws the same: Save Level
         * writes all of them (**MP9**), so none is privileged — the FOCUSED map's header merely
         * starts open and the PERSISTENT one is labelled, because a value you can set is one you
         * must be able to read. Empty states are explicit text, never a blank panel (L12).
         */
        void            DrawContents()          override;

        /** No resource to release. */
        void            Shutdown()              override {}

        /**
         * Re-arm the one-shot listing log: the flag means "logged for the world I am showing", and
         * after a PIE Play/Stop that is a different world. Without this the panel would silently keep
         * claiming the first world's listing.
         */
        void            OnActiveWorldChanged(World* InOld, World* InNew) override;

        PanelWindowStyle GetWindowStyle() const override { return { { 260.f, 400.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** What the context menu asked for, waiting for the draw pass to end. */
        struct PendingMapAction
        {
            EMapAction  Action = EMapAction::None;
            MapId       Map;
            OpaaxString AssetRelPath;
        };

        EditorContext& m_Context;

        PendingMapAction m_Pending;

        // One-shot: Draw() is per-frame, and BOTH empty states are silent — a clean log would otherwise be
        // indistinguishable from an empty panel (L15). Logs the SUCCESS branch once, then never again.
        bool m_bListLogged = false;
    };
}
