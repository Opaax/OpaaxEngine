#pragma once

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;

    // =============================================================================
    // IEditorModule — a game's editor module (Editor.md D10). Compiled ONLY into the game's editor exe.
    //   OnRegister plugs the game's drawers / panels / asset-types / menus / edit-world-systems into the
    //   editor; EditorService invokes it BEFORE the registry seals (before the first world, §2). Symmetric
    //   with the game module's RegisterModule (D9) — registration, not knowledge: the editor never knows the
    //   game's types, the game plugs into the editor's routes.
    // =============================================================================
    class IEditorModule
    {
    public:
        virtual ~IEditorModule() = default;
        virtual void OnRegister(EditorExtensionRegistrar& InRegistrar) = 0;
    };
}
