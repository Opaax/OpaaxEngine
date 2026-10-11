#pragma once

#include "Editor/Extensions/IEditorModule.h"

// =============================================================================
// TestWorldEditorModule — the game's editor module, compiled only into TestWorldEditor.exe. Adds the
// game's drawers / panels / resource types / menus to the generic editor.
// =============================================================================
class TestWorldEditorModule final : public Opaax::Editor::IEditorModule
{
public:
    void OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;
};
