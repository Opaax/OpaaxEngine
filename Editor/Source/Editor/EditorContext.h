#pragma once

namespace Opaax
{
    class IEngine;
    class WorldManager;
    class ResourceManager;

    namespace Editor
    {
        class IEditorUIBackend;         // editor-owned; the context carries it so panels reach it by ctor
        class EditorSelection;          // editor-owned; the single selected entity (Hierarchy writes, Inspector reads)
        class EditorExtensionRegistrar; // editor-owned; the sealed D10 routes (Inspector reads Drawers())

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

            // M2b — the sealed extension routes, so a panel can consume what modules registered (the
            // Inspector walks Drawers()). CONST by construction: Seal() happens at OnModulesRegistered,
            // this context is built at PostEngineStartup, so nothing can register through it. One member
            // serves every route — M2d's AssetTypes() needs no further growth here.
            const EditorExtensionRegistrar& Extensions;
        };
    }
}
