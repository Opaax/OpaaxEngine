#pragma once

namespace Opaax
{
    class IEngine;
    class WorldManager;
    class ResourceManager;

    namespace Editor
    {
        class IEditorUIBackend;   // editor-owned; the context carries it so panels reach it by ctor

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
        };
    }
}
