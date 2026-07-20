#pragma once

#include "Application/IRuntimeModule.h"

namespace Opaax { class World; }

// =============================================================================
// SandboxModule — the game's shared content (Editor.md D9). Linked by BOTH Sandbox.exe (runtime)
// and SandboxEditor.exe (editor). An IRuntimeModule, symmetric with SandboxEditorModule (an
// IEditorModule). Never includes an editor header.
// =============================================================================
class SandboxModule final : public Opaax::IRuntimeModule
{
public:
    // Register the game's components / world subsystems into the engine registries via the
    // registrar's routes. One entry point, one call site per host.
    void OnRegister(Opaax::ModuleRegistrar& InRegistrar) override;

    // Temporary bring-up: populate a world with a few DummyComponent quads so the World -> Renderer
    // path is visible. Shared by both hosts so runtime and editor show the same scene. Not part of the
    // IRuntimeModule contract — a Sandbox-specific extra. Dies with DummyComponent once real content exists.
    void SpawnDemoWorld(Opaax::World& InWorld);
};
