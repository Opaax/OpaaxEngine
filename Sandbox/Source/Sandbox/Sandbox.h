#pragma once

#include "Application/Modules/IRuntimeModule.h"

// =============================================================================
// SandboxModule — the game's shared content (Editor.md D9). Linked by BOTH Sandbox.exe (runtime)
// and SandboxEditor.exe (editor). An IRuntimeModule, symmetric with SandboxEditorModule (an
// IEditorModule). Never includes an editor header.
//
// M5: the module registers TYPES and nothing else. Its old SpawnDemoWorld — which built the demo
// quads in C++ for both hosts — is gone, because the world's CONTENT is now a file the project
// names (Assets/Levels/Main.opaaxlevel -> Assets/Maps/Main.opaaxmap). Types are code; entities
// are data. That split is what makes the world editable without a compiler.
// =============================================================================
class SandboxModule final : public Opaax::IRuntimeModule
{
public:
    // Register the game's components / world subsystems into the engine registries via the
    // registrar's routes. One entry point, one call site per host.
    void OnRegister(Opaax::ModuleRegistrar& InRegistrar) override;
};
