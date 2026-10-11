#pragma once

#include "Editor/Extensions/IEditorModule.h"

// =============================================================================
// TetrisEditorModule — the game's editor module, compiled only into TetrisEditor.exe. Adds the
// game's drawers / panels / resource types / menus to the generic editor.
// =============================================================================
class TetrisEditorModule final : public Opaax::Editor::IEditorModule
{
public:
    void OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;
};
