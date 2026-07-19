#pragma once

namespace Opaax { class ModuleRegistrar; class World; }

// =============================================================================
// SandboxModule — the game's shared content (Editor.md D9). Linked by BOTH Sandbox.exe (runtime)
// and SandboxEditor.exe (editor). Declared here, called by each host. Never includes an editor header.
// =============================================================================
namespace SandboxModule
{
    // Register the game's components / world subsystems into the engine registries via the
    // registrar's routes. One entry point, one call site per host.
    void RegisterModule(Opaax::ModuleRegistrar& InRegistrar);

    // Temporary bring-up: populate a world with a few DummyComponent quads so the World -> Renderer
    // path is visible. Shared by both hosts so runtime and editor show the same scene. Dies with
    // DummyComponent once real content exists.
    void SpawnDemoWorld(Opaax::World& InWorld);
}
