#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/EntityTreeView.h"
#include "Editor/Panels/IEditorPanel.h"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    class Entity;

    OPAAX_LOG_CATEGORY(HierarchyPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * A per-map action from a header's context menu, recorded and run after the draw pass
     * (Remove from Level destroys entities the loop is about to draw).
     */
    enum class EMapAction
    {
        None,
        Save,
        SetPersistent,
        Remove,
        RemoveMissing,  // a manifest entry never mounted (named by path)
        CreateEntity,   // into the clicked map
        DeleteSelected, // the row's menu; acts on the selection
        CreatePrefab,     // the selection becomes a prefab and an instance of it
        RevertPrefab,     // the selected entities go back to their prefab values
        RevertPrefabAll,  // every entity of the touched instances
        Detach            // every selected entity with a parent becomes a root
    };

    /** Enum to string. */
    const char* ToString(EMapAction InAction) noexcept;

    // =============================================================================
    // HierarchyPanel — the "Hierarchy" panel: the active world's entities as a tree, grouped by map.
    //   The only writer of EditorSelection (the Inspector reads it).
    //   Groups come from the Level's mounted maps (so empty maps show), and entities are bucketed by
    //   EntityMeta::OwnerMap; entities of no map get a "(runtime - not saved)" group.
    //   Only roots are bucketed; EntityTreeView draws children under them. Drops are applied after
    //   the walk.
    //   Per-map actions (save, set persistent, remove) are on each header's context menu, and the
    //   unsaved * is shown per map (from EditorLevelDocument's throttled cache).
    //   Registered like any other panel (no special route).
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
         * The context menu of a map header (the bodies are MapOps', shared with the File menu).
         * Entries that do not apply are disabled.
         * @param InMapId       Valid when InMounted
         * @param InMounted     False for the runtime group (no menu)
         * @param InPersistent  The level's persistent map
         * @param InMissing     A manifest entry whose file never loaded: only "remove" applies
         */
        void DrawMapContextMenu(MapId InMapId, const OpaaxString& InAssetRelPath,
                                bool InMounted, bool InPersistent, bool InMissing);

        /**
         * The context menu of an entity row. The row is selected first, so Delete acts on the selection
         * (like the Delete key and the Edit menu). Queued like every action here.
         */
        void DrawEntityContextMenu(Entity InEntity);

        /**
         * Runs what the context menu queued, after the draw pass (the draw reads the world; these write it).
         */
        void RunPendingAction();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void            Startup()               override {}

        /** Nothing the world render depends on. */
        void            OnPreRender()           override {}

        /**
         * One header per mounted map, one row per entity. A click selects, a right-click on a header
         * opens its menu. The focused map's header starts open; the persistent one is labelled.
         * Empty states are shown as text.
         */
        void            DrawContents()          override;

        /** Nothing to release. */
        void            Shutdown()              override {}

        /**
         * Re-arms the one-time listing log (after Play/Stop it is a different world).
         */
        void            OnActiveWorldChanged(World* InOld, World* InNew) override;

        PanelWindowStyle GetWindowStyle() const override { return { { 260.f, 400.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** The action the context menu asked for, run after the draw pass. */
        struct PendingMapAction
        {
            EMapAction  Action = EMapAction::None;
            MapId       Map;
            OpaaxString AssetRelPath;
        };

        EditorContext& m_Context;

        PendingMapAction m_Pending;

        /** The rows, and the drops they store. */
        EntityTreeView m_Tree;

    };
}
