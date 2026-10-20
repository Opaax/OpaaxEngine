#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/Commands/EditorCommandConcept.h"   // NoParams
#include "Editor/Operation/EntityOps.h"             // TransformDelta

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The editor's native commands, one struct per action. Each has a Params type and
    //   `void Execute(EditorContext&, const Params&)`, is registered under a tag from
    //   EditorNativeCommandsTags.hpp, and is dispatched by tag (menus, Resource Browser, shortcuts),
    //   exactly like a game module's commands. A command only uses the context and its params.
    //   Stateless: the registry builds one per dispatch.
    // =============================================================================

    // =============================================================================
    // Params
    // =============================================================================

    /** An absolute path to a .opaaxmap. Distinct from LevelPathParams so the type check tells them apart. */
    struct MapPathParams
    {
        OpaaxString AbsPath;

        /** Unsaved changes of the open level are lost without asking (a script has nobody to ask). */
        bool bDiscardEdits = false;
    };

    /** An absolute path to a .opaaxlevel. */
    struct LevelPathParams
    {
        OpaaxString AbsPath;

        /** Unsaved changes of the open level are lost without asking (a script has nobody to ask). */
        bool bDiscardEdits = false;
    };

    /** A new level's .opaaxlevel and the .opaaxmap it starts with, both absolute. */
    struct NewLevelParams
    {
        OpaaxString LevelAbsPath;

        /** Empty: Maps/<the level's name>.opaaxmap. */
        OpaaxString MapAbsPath;

        /** Unsaved changes of the open level are lost without asking (a script has nobody to ask). */
        bool bDiscardEdits = false;
    };

    /** An absolute path to a .opaaxprefab. */
    struct PrefabPathParams
    {
        OpaaxString AbsPath;
    };

    /** An absolute path to a .opaaxui. */
    struct UICanvasPathParams
    {
        OpaaxString AbsPath;
    };

    /** An absolute path to an .opaaxasset. Empty: the asset whose window has the focus. */
    struct AssetPathParams
    {
        OpaaxString AbsPath;
    };

    /** Where a new asset goes and what it is: a folder (absolute) and a registered class's name. */
    struct NewAssetParams
    {
        OpaaxString   FolderAbsPath;
        OpaaxStringID ClassName;
    };

    /** Which panel a panel command acts on (the id from its PanelDesc). */
    struct PanelIdParams
    {
        OpaaxStringID PanelId;
    };

    /**
     * Which map a command authors into. Invalid means the focused map (a menu entry has nothing else
     * to name); the command refuses when there is none.
     */
    struct MapIdParams
    {
        MapId Map;
    };

    /** A new name for the primary selection. */
    struct EntityNameParams
    {
        OpaaxString Name;
    };

    /**
     * Which component type a command acts on, by authoring name (plain data a key binding could carry).
     */
    struct ComponentTypeParams
    {
        OpaaxStringID TypeName;
    };

    // =============================================================================
    // App
    // =============================================================================

    /**
     * Closes the editor through Window::RequestClose (same path as clicking the X). No params: the
     * window comes from the context, so a key binding can trigger it too.
     */
    struct QuitCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Asks for a folder, then exports the edited game into it in the background (ExportOps): the
     * release build of the game, its content and the engine's, ready to run on another machine.
     */
    struct ExportGameCommand
    {
        using Params = NoParams;
        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Minimizes the window (the first caption button).
     */
    struct MinimizeWindowCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Maximizes the window, or restores it if already maximized (caption button and bar double-click).
     */
    struct ToggleMaximizeWindowCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Shows or hides one panel (the Window menu entries and the window's close button). One command
     * for all panels; the panel id is the payload.
     */
    struct TogglePanelCommand
    {
        using Params = PanelIdParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    // =============================================================================
    // Undo
    //   These record nothing themselves. The Play-mode check (MapOps::CanEdit) is done here, since the
    //   undo stack knows nothing about worlds.
    // =============================================================================

    /** Undoes the last step. Refused during Play. */
    struct UndoCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Redoes the step an Undo removed (restores what was recorded, does not re-run the action). */
    struct RedoCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Play in editor
    //   The four Play actions as commands, so the toolbar, the F-keys and the Play menu make the same
    //   call, and the keys can be rebound. Each forwards to the context's PlayInEditor.
    // =============================================================================

    /** Clones the edit world and runs it. Refused unless in Edit (PlayInEditor logs it). */
    struct PlayCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Pauses if playing, resumes if paused. */
    struct TogglePauseCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Ticks one frame, then stays paused. Refused unless Paused. */
    struct StepCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Discards the Play clone and restores the edit world. Refused in Edit. */
    struct StopCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Entity
    //   Bodies are in EntityOps, shared by the Edit menu, the Hierarchy menus and the viewport keys.
    // =============================================================================

    /**
     * Creates an empty entity in the focused map and selects it. (The Hierarchy's header menu calls
     * EntityOps directly to name a map.)
     */
    struct CreateEntityCommand
    {
        using Params = MapIdParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Destroys everything selected. Refused during Play. */
    struct DeleteSelectedCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Renames the primary selection (the Inspector's name field, on Enter or focus loss). */
    struct RenameSelectedCommand
    {
        using Params = EntityNameParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Adds a component to the primary selection (the Inspector's Add popup). */
    struct AddComponentCommand
    {
        using Params = ComponentTypeParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Removes one (the Remove popup). Essential types are refused by the registry entry. */
    struct RemoveComponentCommand
    {
        using Params = ComponentTypeParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /**
     * Applies one drag frame to the selection (the gizmo). Params are EntityOps::TransformDelta.
     * Dispatched every frame of a drag and records nothing: the viewport records one EntityTransform
     * for the whole drag.
     */
    struct TransformSelectedCommand
    {
        using Params = EntityOps::TransformDelta;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Frames the selection with the editor camera. Refused outside Edit (see EntityOps). */
    struct FocusSelectedCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Gizmo
    //   One command per mode, since a key binding carries only a tag (W/E/R). Not edits, so no
    //   Play-mode check.
    // =============================================================================

    /** Move mode (the default). */
    struct GizmoTranslateCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Rotate mode: turns the selection about the gizmo's pivot. */
    struct GizmoRotateCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Scale mode: scales the selection about the gizmo's pivot. */
    struct GizmoScaleCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Map
    // =============================================================================

    /** Asks where, then runs NewMapAtCommand. */
    struct NewMapCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Creates an empty .opaaxmap at Params::AbsPath, adds it to the open level, and focuses it. The file
     * is written immediately, with its own map id. Refuses an existing path (use Open Map or Add Map).
     */
    struct NewMapAtCommand
    {
        using Params = MapPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Asks for a .opaaxmap, then runs OpenMapAtCommand. */
    struct OpenMapCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Edits the map at Params::AbsPath (File > Open Map and a Resource Browser double-click).
     * If the map is already in the world, nothing loads: the document is retargeted and the selection
     * kept. A map from no open level gets its own world with an empty Level, after asking before
     * unsaved changes are lost.
     */
    struct OpenMapAtCommand
    {
        using Params = MapPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Writes the focused map. Save Level writes them all. */
    struct SaveMapCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Asks where, then runs SaveMapAsAtCommand. */
    struct SaveMapAsCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Writes the focused map to Params::AbsPath. The level names that file from then on (its file is saved
     * at once), the old file stays as it was last saved, and the focus follows the map.
     */
    struct SaveMapAsAtCommand
    {
        using Params = MapPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    // =============================================================================
    // Prefab
    // =============================================================================

    /**
     * Places one instance of the prefab at Params::AbsPath into the focused map
     * (EntityOps::InstantiatePrefab).
     */
    struct InstantiatePrefabAtCommand
    {
        using Params = PrefabPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /**
     * Asks where, writes the selection there as a prefab, and replaces it with an instance. The
     * dialog defaults to the primary entity's name.
     */
    struct CreatePrefabFromSelectionCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Which entities a revert covers. */
    struct PrefabRevertParams
    {
        /** False: exactly the selection. True: every entity of the instances it touches. */
        bool bWholeInstance = false;
    };

    /**
     * Reverts the selected instance entities to their prefab's values (EntityOps::RevertToPrefab).
     */
    struct RevertToPrefabCommand
    {
        using Params = PrefabRevertParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Opens Params::AbsPath in the Prefab panel, in its own world. */
    struct OpenPrefabAtCommand
    {
        using Params = PrefabPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Writes the open prefab (the panel's button and Ctrl+S). */
    struct SavePrefabCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Asks where, then runs SavePrefabAsVariantAtCommand. The dialog defaults to "<Base>Variant" next to the
     * base.
     */
    struct SavePrefabAsVariantCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Writes Params::AbsPath as a variant of the open prefab (its unsaved edits become the variant's
     * overrides; the base file is left as it is), then opens the variant.
     */
    struct SavePrefabAsVariantAtCommand
    {
        using Params = PrefabPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /**
     * Undo/redo in the prefab document's own history (Ctrl+Z/Y while the panel is focused). No Play
     * check: the prefab world is always Edit.
     */
    struct UndoPrefabCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    struct RedoPrefabCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Destroys the selection in the prefab document's world and records it on its stack (Delete while
     * the panel is focused). No Play check.
     */
    struct DeletePrefabSelectionCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Level
    // =============================================================================

    /** Asks where, then runs NewLevelAtCommand with the level's default map. */
    struct NewLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Creates a level of one empty map and opens it for editing, after asking before unsaved changes
     * are lost. Both files are written at once; an existing file is refused (Open Level and Add Map are
     * for those).
     */
    struct NewLevelAtCommand
    {
        using Params = NewLevelParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Asks for a .opaaxlevel, then runs OpenLevelAtCommand. */
    struct OpenLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Opens a .opaaxlevel into a new world (browser double-click and File > Open Level), after asking
     * before unsaved changes are lost.
     */
    struct OpenLevelAtCommand
    {
        using Params = LevelPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Writes the level file and every map it loads. */
    struct SaveLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Rewrites the level file and every map it loads in the current format, changed or not (after a
     * format change, or for files written by hand or by a tool).
     */
    struct ResaveLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Writes the open .opaaxsheet. Used by the panel's button and Ctrl+S (when the sheet panel has
     * focus; see EditorService::HandleAuthoringShortcuts).
     */
    struct SaveSheetCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Asks where, then runs NewUIAtCommand. */
    struct NewUICommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Creates an empty .opaaxui at Params::AbsPath (a root panel, the project's reference height) and opens
     * it in the UI panel. Refuses an existing file.
     */
    struct NewUIAtCommand
    {
        using Params = UICanvasPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Writes the open .opaaxui and reloads it. */
    struct SaveUICommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Deletes the UI panel's selected widget (Delete while that panel is focused). */
    struct DeleteUIWidgetCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Add Map to Level: asks for a map file, then runs AddMapToLevelAtCommand. (Removing a map and choosing
     * the persistent one are on the Hierarchy's map headers.)
     */
    struct AddMapToLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Adds the existing map at Params::AbsPath to the open level: its entities load, and the level file is
     * saved at once. The focus stays where it was.
     */
    struct AddMapToLevelAtCommand
    {
        using Params = MapPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    // =============================================================================
    // Assets — any .opaaxasset, whatever its class (AssetEditors)
    // =============================================================================

    /** Opens the asset in a window of its own, or brings its window forward. */
    struct OpenAssetCommand
    {
        using Params = AssetPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /**
     * Writes a new asset of Params::ClassName in Params::FolderAbsPath, named New<Class> (numbered when
     * taken), and opens it.
     */
    struct NewAssetCommand
    {
        using Params = NewAssetParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Writes the asset (empty path: the focused one) and reloads it, so a running game reads it. */
    struct SaveAssetCommand
    {
        using Params = AssetPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Writes every open asset with unsaved changes. */
    struct SaveAllAssetsCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Closes the asset's window (empty path: the focused one), asking before losing unsaved changes. */
    struct CloseAssetCommand
    {
        using Params = AssetPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Steps the asset's own history back (empty path: the focused one). */
    struct UndoAssetCommand
    {
        using Params = AssetPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Steps the asset's own history forward (empty path: the focused one). */
    struct RedoAssetCommand
    {
        using Params = AssetPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };
}
