#pragma once

#include "Application/Modules/IRuntimeModule.h"

// =============================================================================
// __NAME__Module — the game's shared content module (Editor.md D9). Linked by BOTH __NAME__.exe
// (runtime) and __NAME__Editor.exe (editor), so both hosts register the same game content.
// An IRuntimeModule, symmetric with __NAME__EditorModule (an IEditorModule). Never includes an
// editor header.
// =============================================================================
class __NAME__Module final : public Opaax::IRuntimeModule
{
public:
    /**
     * Register the game's components / world subsystems into the engine registries via the
     * registrar's routes. One entry point, one call site per host.
     */
    void OnRegister(Opaax::ModuleRegistrar& InRegistrar) override;
};
