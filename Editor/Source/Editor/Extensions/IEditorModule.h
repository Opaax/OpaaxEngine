#pragma once

#include "Application/Modules/IModule.h"

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;

    // =============================================================================
    // IEditorModule — a game's editor module, compiled only into the game's editor exe. OnRegister
    //   adds the game's drawers, panels, resource types, menus and edit-world systems to the editor,
    //   before the registry seals. Mirrors the runtime IRuntimeModule::OnRegister.
    // =============================================================================
    class IEditorModule : public IModule
    {
    public:
        virtual ~IEditorModule() = default;
        virtual void OnRegister(EditorExtensionRegistrar& InRegistrar) = 0;
    };
}
