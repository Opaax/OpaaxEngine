#pragma once

#include "Application/Modules/IRuntimeModule.h"

// =============================================================================
// __NAME__Module — the game's shared content module. Linked by both __NAME__.exe (runtime) and
// __NAME__Editor.exe (editor), so both register the same game content. Never includes an editor
// header.
// =============================================================================
class __NAME__Module final : public Opaax::IRuntimeModule
{
public:
    /**
     * Registers the game's components and world subsystems through the registrar's routes.
     */
    void OnRegister(Opaax::ModuleRegistrar& InRegistrar) override;
};
