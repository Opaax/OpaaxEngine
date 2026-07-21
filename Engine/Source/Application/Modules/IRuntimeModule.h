#pragma once

#include "Application/Modules/IModule.h"

namespace Opaax
{
    class ModuleRegistrar;

    // =============================================================================
    // IRuntimeModule — a game's runtime module (Editor.md D9). Linked by the runtime exe AND the editor exe
    //   (both hosts register the same game content). OnRegister plugs the game's components / world
    //   subsystems into the engine registries via the registrar's routes; the host invokes it from
    //   OpaaxApplication::RegisterModules, before any world exists. Symmetric with IEditorModule (D10):
    //   registration, not knowledge — the engine never knows the game's types, the game plugs into the
    //   engine's routes.
    // =============================================================================
    class IRuntimeModule : public IModule
    {
    public:
        virtual void OnRegister(ModuleRegistrar& InRegistrar) = 0;
    };
}
