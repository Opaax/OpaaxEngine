#pragma once

namespace Opaax
{
    class IEngine;
    class WorldManager;
    class ResourceManager;
    class IPaths;
    class IFileSystem;

    namespace Editor
    {
        class IEditorUIBackend;         // editor-owned; the context carries it so panels reach it by ctor
        class EditorSelection;          // editor-owned; the single selected entity (Hierarchy writes, Inspector reads)
        class PlayInEditor;             // editor-owned; the PIE state machine (toolbar + reserved keys drive it)
        class InputRoute;               // editor-owned; whether the engine is being fed (D5 steps 2 + 4)
        class EditorMapDocument;        // editor-owned; WHICH map is open and whether it changed (M5)
        class EditorLevelDocument;      // editor-owned; WHICH level is open and whether it changed
        class EditorExtensionRegistrar; // editor-owned; the sealed D10 routes (Inspector reads Drawers())
        class EditorPaths;              // editor-owned IPaths subclass; the editor-space directories

        // =============================================================================
        // EditorContext — a flat struct of engine-side references (Editor.md D3). Resolved ONCE by
        //   EditorService (the editor's composition root) and injected BY CONSTRUCTOR into every panel
        //   and drawer. Nothing downstream ever sees the AppServiceLocator or a static.
        //
        //   Built AFTER engine startup (PostEngineStartup): the subsystems it references are created in
        //   Engine::Startup and then live for the engine's lifetime, so these refs are stable. Growing
        //   set — AssetRegistry, EditorState, EditorEventBus join as their milestones land.
        //
        //   UIBackend is the one editor-owned member (not an engine subsystem): the ViewportPanel needs
        //   it to turn its FBO into an ImGui image (GetViewportImage). EditorService builds it BEFORE the
        //   context so this reference is valid (see EditorService::Initialize ordering).
        // =============================================================================
        struct EditorContext
        {
            IEngine&          Engine;
            WorldManager&     Worlds;
            ResourceManager&  Resources;
            IEditorUIBackend& UIBackend;
            EditorSelection&  Selection;   // M2a — Hierarchy writes, Inspector reads

            // M4 S5 — the PIE state machine. Here rather than inside the toolbar panel because the
            // reserved keys (EditorService::RouteInput) drive the very same object, so the buttons
            // and the shortcuts cannot disagree about what is playing.
            PlayInEditor&     PIE;

            // M-Input S2 — the one place that answers "is the engine being fed?". RouteInput gates
            // on it and the Input panel displays it, so the behaviour and the readout cannot drift.
            // Named Route, not InputRoute: a member sharing its type's name shadows it in-struct.
            InputRoute&       Route;

            // WM1a — WHICH `.opaaxlevel` the session has open, and whether its manifest changed.
            // What a session holds is a LEVEL, not a map: the maps being edited are its maps. The
            // manifest itself is NOT here — it lives in the world's Level, one owner.
            EditorLevelDocument& LevelDocument;

            // M5 — the open `.opaaxmap`: its path, its MapId, and whether the world still matches
            // what was last written. Here rather than inside a Save command so the menu, the title
            // bar and any future panel all read ONE answer — the same reason PIE and Route are here.
            EditorMapDocument& MapDocument;

            // M2b — the sealed extension routes, so a panel can consume what modules registered (the
            // Inspector walks Drawers(), the Resource Browser ResourceTypes()). CONST by construction:
            // Seal() happens at OnModulesRegistered, this context is built at PostEngineStartup, so
            // nothing can register through it. One member serves every route.
            const EditorExtensionRegistrar& Extensions;

            // M2d — the browser resolves its roots from paths and walks them through the file system.
            // Both are resolved ONCE by EditorService: a panel never touches the locator (D3).
            const IPaths&      Paths;
            const IFileSystem& FileSystem;

            // The editor's own per-project space (<ProjectRoot>/Editor/Assets). NULL is a real state,
            // not an error: EditorApplication installs a plain Paths when no edited project is declared,
            // and then there simply is no editor space to browse.
            const EditorPaths* EditorPathsOrNull;
        };
    }
}
