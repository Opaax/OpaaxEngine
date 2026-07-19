#pragma once

namespace Opaax
{
    class IEngine;
    class WorldManager;
    class ResourceManager;

    namespace Editor
    {
        // =============================================================================
        // EditorContext — a flat struct of engine-side references (Editor.md D3). Resolved ONCE by
        //   EditorService (the editor's composition root) and injected BY CONSTRUCTOR into every panel
        //   and drawer. Nothing downstream ever sees the AppServiceLocator or a static.
        //
        //   Built AFTER engine startup (PostEngineStartup): the subsystems it references are created in
        //   Engine::Startup and then live for the engine's lifetime, so these refs are stable. Growing
        //   set — AssetRegistry, EditorState, EditorEventBus join as their milestones land.
        // =============================================================================
        struct EditorContext
        {
            IEngine&         Engine;
            WorldManager&    Worlds;
            ResourceManager& Resources;
        };
    }
}
