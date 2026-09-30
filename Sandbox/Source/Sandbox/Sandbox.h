#pragma once

#include "Application/Modules/IRuntimeModule.h"

// =============================================================================
// SandboxModule — the game's shared content. Linked by both Sandbox.exe (runtime) and
// SandboxEditor.exe (editor). Never includes an editor header.
// Registers types only; the world's content is a file (Assets/Levels/Main.opaaxlevel).
// =============================================================================
class SandboxModule final : public Opaax::IRuntimeModule
{
public:
    // Registers the game's components and world subsystems through the registrar's routes.
    void OnRegister(Opaax::ModuleRegistrar& InRegistrar) override;
};
