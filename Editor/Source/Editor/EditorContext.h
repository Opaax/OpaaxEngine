#pragma once

namespace Opaax
{
    class IEngine;
    class WorldManager;
    class ResourceManager;
    class IPaths;
    class IFileSystem;
    class IConfigSystem;
    class Window;

    namespace Editor
    {
        class IEditorGui;               // the UI backend, owns the UI pass
        class IEditorUIBackend;         // passed to panels by constructor
        class IEditorDialogs;           // file pickers and message boxes
        class IEditorWidgets;           // value editors used by drawers
        class EditorCamera;             // the editor camera for Edit worlds
        class EditorSelection;          // what is selected (Hierarchy + viewport write, Inspector reads)
        class EditorResourceEvents;     // "a document was saved", two-phase
        class EditorPrefabDocument;     // the open prefab and its world
        class EditorViewport;           // viewport size in pixels
        class EditorGizmo;              // transform gizmo state
        class EditorUndo;               // undo/redo stacks
        class PlayInEditor;             // Play In Editor state machine
        class InputRoute;               // whether the engine is fed input
        class EditorMapDocument;        // the focused map
        class EditorLevelDocument;      // the open level and whether it changed
        class EditorSpriteSheetDocument;// the open .opaaxsheet and its data
        class EditorAnimationClipDocument; // the open .opaaxclip and its data
        class EditorAnimationLibraryDocument; // the open .opaaxanim and its data
        class EditorFontFamilyDocument;       // the open .opaaxfont and its data
        class EditorUICanvasDocument;         // the open .opaaxui and its canvas
        class EditorMoveModeDocument;         // the open .opaaxmovemode and its data
        class EditorMoverDocument;            // the open .opaaxmover and its data
        class EditorInputActionDocument;      // the open .opaaxaction and its data
        class EditorInputMappingContextDocument; // the open .opaaxinputmap and its data
        class EditorDataAssetDocument;        // the open .opaaxdata, any registered type
        class EditorExtensionRegistrar; // the sealed extension routes
        class EditorPaths;              // IPaths subclass: the editor directories
        class EditorPanels;             // live panels and visibility
        class GameExport;               // exporting the game, in the background
        class ResourcePreview;          // which resource a double-click asked to preview

        // =============================================================================
        // EditorContext — references to editor and engine systems, built once by EditorService and
        //   passed by constructor to every panel and drawer (nothing uses the service locator or statics).
        //   Built after engine startup, so every reference stays valid for the editor's lifetime.
        // =============================================================================
        struct EditorContext
        {
            IEngine&          Engine;
            WorldManager&     Worlds;
            ResourceManager&  Resources;

            // The UI backend: host chrome (menu bar, panel windows) and the input capture checks.
            IEditorGui&       Gui;

            // Gui.Backend(), separate: a panel that only turns a texture into an image needs only this.
            IEditorUIBackend& UIBackend;

            // Modal dialogs: file pickers and confirmations. The answer comes through a callback
            // (the native implementation calls it immediately).
            IEditorDialogs&   Dialogs;

            // Gui.Widgets(): the value editors drawers use.
            IEditorWidgets&   Widgets;

            EditorSelection&  Selection;   // Hierarchy writes, Inspector reads

            // "A document was saved", in two phases. Written by the Save ops, read by whoever cares.
            EditorResourceEvents& ResourceEvents;

            // The open .opaaxprefab: its data and its world (the ResourceManager's copy is what placed
            // instances were built from, so it is not edited directly).
            EditorPrefabDocument& PrefabDocument;

            // The viewport size in pixels, measured by the viewport panel (focus-selected needs its aspect).
            EditorViewport&   Viewport;

            // The editor camera: sets World::CameraView for Edit worlds. Here (not in the viewport panel)
            // because it must survive a Play session.
            EditorCamera&     Camera;

            // The transform gizmo's state. Its subject is the selection, and its mode has editor-wide shortcuts.
            EditorGizmo&      Gizmo;

            // The undo/redo stacks. Written by command dispatch, read by the Edit menu and the shortcuts.
            EditorUndo&       Undo;

            // The Play In Editor state machine, shared by the toolbar buttons and the reserved keys.
            PlayInEditor&     PIE;

            // Whether the engine is being fed input. RouteInput and the Input panel both read it.
            // Named Route: a member with its type's name would shadow the type.
            InputRoute&       Route;

            // The open .opaaxlevel, and whether its manifest changed (the manifest itself is in the world's Level).
            EditorLevelDocument& LevelDocument;

            // The focused map (see EditorMapDocument).
            EditorMapDocument& MapDocument;

            // The open sprite sheet. Owns its data: the ResourceManager's copy is what the renderer draws,
            // so editing it would change the running game.
            EditorSpriteSheetDocument& SheetDocument;

            // The open animation clip. Owns its data (a playing entity uses the ResourceManager's copy).
            // Save publishes it.
            EditorAnimationClipDocument& ClipDocument;

            // The open animation library (short names -> clips). Owns its data.
            EditorAnimationLibraryDocument& LibraryDocument;

            // The open font family ((subset, weight, width, slant) -> .ttf). Owns its data.
            EditorFontFamilyDocument& FamilyDocument;

            // The open .opaaxui. Owns a real UICanvas: the panel previews it by rendering it.
            EditorUICanvasDocument& UICanvasDocument;

            // The open movement tuning (.opaaxmovemode).
            EditorMoveModeDocument& MoveModeDocument;

            // The open mover (short names -> tunings).
            EditorMoverDocument& MoverDocument;

            // The open input action. It names no keys (mapping contexts do).
            EditorInputActionDocument& InputActionDocument;

            // The open mapping context: which keys reach which actions (what a rebind edits).
            EditorInputMappingContextDocument& InputMapDocument;

            // The open data asset (.opaaxdata), whatever struct it holds.
            EditorDataAssetDocument& DataAssetDocument;

            // The sealed extension routes (Inspector: Drawers(), Resource Browser: ResourceTypes()).
            // Const: registration is closed before this context is built.
            const EditorExtensionRegistrar& Extensions;

            // The live panels and their visibility (Extensions.Panels() is the list of descriptions).
            EditorPanels& Panels;

            // Which resource a double-click asked to preview (written by the type's activate callback,
            // read by the Preview panel).
            ResourcePreview& Preview;

            // Paths and file system for the Resource Browser. Resolved once by EditorService.
            const IPaths&      Paths;
            const IFileSystem& FileSystem;

            // The engine's config registry (the Config panel lists it). May grow at any time: don't cache it.
            IConfigSystem& Configs;

            // The editor window, so commands (e.g. Quit) can use it from a key binding too.
            Window& MainWindow;

            // The editor's per-project space (<ProjectRoot>/Editor/Assets). May be null (no edited project).
            const EditorPaths* EditorPathsOrNull;

            // Exporting the edited game into a folder that runs elsewhere (File > Export Game, automation).
            GameExport& Export;
        };
    }
}
