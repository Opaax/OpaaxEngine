#pragma once

namespace Opaax
{
    class IEngine;
    class WorldManager;
    class ResourceManager;
    class IPaths;
    class IFileSystem;
    class IConfigSystem;
    class IStatsService;
    class Window;

    namespace Editor
    {
        class IEditorGui;               // editor-owned; the UI backend seam and the owner of the UI pass
        class IEditorUIBackend;         // editor-owned; the context carries it so panels reach it by ctor
        class IEditorDialogs;           // editor-owned; file pickers and message boxes
        class EditorCamera;             // editor-owned; how the author is looking at an Edit world
        class EditorSelection;          // editor-owned; what is selected (Hierarchy + viewport write, Inspector reads)
        class EditorViewport;           // editor-owned; how big the viewport image is, in pixels
        class EditorGizmo;              // editor-owned; the transform handles' grab state
        class PlayInEditor;             // editor-owned; the PIE state machine (toolbar + reserved keys drive it)
        class InputRoute;               // editor-owned; whether the engine is being fed (D5 steps 2 + 4)
        class EditorMapDocument;        // editor-owned; WHICH map is open and whether it changed (M5)
        class EditorLevelDocument;      // editor-owned; WHICH level is open and whether it changed
        class EditorExtensionRegistrar; // editor-owned; the sealed D10 routes (Inspector reads Drawers())
        class EditorPaths;              // editor-owned IPaths subclass; the editor-space directories
        class EditorPanels;             // editor-owned; the LIVE panels and their visibility
        class ResourcePreview;          // editor-owned; WHICH resource a double-click asked to see

        // =============================================================================
        // EditorContext — a flat struct of engine-side references (Editor.md D3). Resolved ONCE by
        //   EditorService (the editor's composition root) and injected BY CONSTRUCTOR into every panel
        //   and drawer. Nothing downstream ever sees the AppServiceLocator or a static.
        //
        //   Built AFTER engine startup (PostEngineStartup): the subsystems it references are created in
        //   Engine::Startup and then live for the engine's lifetime, so these refs are stable. Growing
        //   set — AssetRegistry, EditorState, EditorEventBus join as their milestones land.
        //
        //   Gui and UIBackend are editor-owned rather than engine subsystems: the ViewportPanel needs the
        //   second to turn its FBO into an image (GetViewportImage), and menus/panels draw their chrome
        //   through the first. EditorService brings the UI up BEFORE building the context, so both
        //   references are valid (see EditorService::Initialize ordering).
        // =============================================================================
        struct EditorContext
        {
            IEngine&          Engine;
            WorldManager&     Worlds;
            ResourceManager&  Resources;

            // The UI backend seam — host chrome (menu bar, panel window) and the capture predicates.
            // Here so a menu node or a panel draws through it without EditorService threading it down.
            IEditorGui&       Gui;

            // Gui.Backend(). Kept as its own member because it is the NARROWER dependency: a panel
            // that only turns a texture into an image has no business with the rest of the UI.
            IEditorUIBackend& UIBackend;

            // The modal seam — "where do I save this?", "are you sure?". Beside Gui rather than a
            // section of it for UIBackend's reason: a command that asks where to save a file has no
            // business with the menu bar. The answer arrives by CONTINUATION; the native
            // implementation fires it inline, so a call site may capture this context.
            IEditorDialogs&   Dialogs;

            EditorSelection&  Selection;   // M2a — Hierarchy writes, Inspector reads

            // ② — the viewport's pixel size, measured by the panel. Here because the WRITER is a
            // panel and the READER is a menu command: focus-selected needs the aspect to frame a
            // wide selection, and there is no typed route to a panel (the ResourcePreview shape).
            EditorViewport&   Viewport;

            // ① — the Edit-side producer of World::CameraView, opposite the engine's CameraManager.
            // Here rather than inside the ViewportPanel because it must OUTLIVE a PIE cycle: the panel
            // is a panel, this is the session's viewpoint. It is also what ② will ask for the camera
            // when it turns a click into a world position.
            EditorCamera&     Camera;

            // ③ — the transform handles' grab state. Here rather than inside the ViewportPanel for
            // the reason Selection is: its subject is the SELECTION, which no panel owns (SEL7), and
            // the gizmo MODE is set by an editor-wide shortcut.
            EditorGizmo&      Gizmo;

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

            // The LIVE panels — instances and visibility. Distinct from Extensions.Panels(), which is
            // the sealed list of DESCRIPTIONS this was built from.
            EditorPanels& Panels;

            // ④b — WHICH resource a double-click asked to look at. A resource type's activate closure
            // writes it and the Preview panel reads it; here because those are different objects and
            // a closure gets no other way to reach one (the Selection/PIE/MapDocument shape).
            ResourcePreview& Preview;

            // M2d — the browser resolves its roots from paths and walks them through the file system.
            // Both are resolved ONCE by EditorService: a panel never touches the locator (D3).
            const IPaths&      Paths;
            const IFileSystem& FileSystem;

            // The engine's config registry — what the Config panel lists. Read LIVE, never snapshotted:
            // Get<T>() auto-registers, so a system reading its config on a later frame grows it.
            IConfigSystem& Configs;

            // ④ — what the last frame cost. An APP SERVICE, not something on IEngine (I4): the
            // engine is a consumer of it too. Never null; with stats off it answers an empty frame,
            // which the Stats panel renders as "nothing measured" without a special case.
            IStatsService& Stats;

            // The window the editor is drawn in — resolved once by EditorService, like Paths and
            // FileSystem beside it. Here because a command that needs something the composition
            // root has to hand IN is a command nothing but a menu can invoke: a key binding carries
            // a tag and no payload, so QuitCommand could never have been bound to one.
            Window& MainWindow;

            // The editor's own per-project space (<ProjectRoot>/Editor/Assets). NULL is a real state,
            // not an error: EditorApplication installs a plain Paths when no edited project is declared,
            // and then there simply is no editor space to browse.
            const EditorPaths* EditorPathsOrNull;
        };
    }
}
