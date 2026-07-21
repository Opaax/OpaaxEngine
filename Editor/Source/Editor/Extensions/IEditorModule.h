#pragma once

#include "Application/Modules/IModule.h"

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;

    // =============================================================================
    // IEditorModule — a game's editor module (Editor.md D10). Compiled ONLY into the game's editor exe.
    //   OnRegister plugs the game's drawers / panels / asset-types / menus / edit-world-systems into the
    //   editor; EditorService invokes it BEFORE the registry seals (before the first world, §2). Symmetric
    //   with the runtime module's IRuntimeModule::OnRegister (D9) — registration, not knowledge: the editor
    //   never knows the game's types, the game plugs into the editor's routes. Shares IModule with it.
    // =============================================================================
    class IEditorModule : public IModule
    {
    public:
        virtual ~IEditorModule() = default;
        virtual void OnRegister(EditorExtensionRegistrar& InRegistrar) = 0;
    };
}
