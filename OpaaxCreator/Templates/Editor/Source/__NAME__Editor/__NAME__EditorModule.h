#pragma once

#include "Editor/Extensions/IEditorModule.h"

// =============================================================================
// __NAME__EditorModule — the game's editor module (Editor.md D10). Compiled ONLY into
// __NAME__Editor.exe. Plugs the game's drawers / panels / resource-types / menus into the
// generic editor; the editor never knows the game's types.
// =============================================================================
class __NAME__EditorModule final : public Opaax::Editor::IEditorModule
{
public:
    void OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;
};
