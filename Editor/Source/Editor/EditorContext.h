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
        class EditorUICanvasDocument;   // the open .opaaxui and its canvas
        class EditorExtensionRegistrar; // the sealed extension routes
        class EditorPaths;              // IPaths subclass: the editor directories
        class EditorPanels;             // live panels and visibility
        class GameExport;               // exporting the game, in the background
        class AssetEditors;             // the open .opaaxasset files, a window each

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

            // The open .opaaxui. Owns a real UICanvas: the panel previews it by rendering it.
            EditorUICanvasDocument& UICanvasDocument;

            // The sealed extension routes (Inspector: Drawers(), Resource Browser: AssetTypes()).
            // Const: registration is closed before this context is built.
            const EditorExtensionRegistrar& Extensions;

            // The live panels and their visibility (Extensions.Panels() is the list of descriptions).
            EditorPanels& Panels;

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

            // The open assets, whatever their class: one window, one history and one Save each.
            AssetEditors& Assets;
        };
    }
}
