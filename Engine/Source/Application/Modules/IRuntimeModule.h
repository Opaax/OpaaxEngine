#pragma once

#include "Application/Modules/IModule.h"

namespace Opaax
{
    class ModuleRegistrar;

    // =============================================================================
    // IRuntimeModule — a game's runtime module, used by both the game and the editor.
    //   OnRegister adds the game's components and world subsystems to the engine registries.
    //   Called before any world exists.
    // =============================================================================
    class IRuntimeModule : public IModule
    {
    public:
        virtual void OnRegister(ModuleRegistrar& InRegistrar) = 0;
    };
}
