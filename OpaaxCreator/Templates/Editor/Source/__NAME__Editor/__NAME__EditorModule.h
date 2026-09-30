#pragma once

#include "Editor/Extensions/IEditorModule.h"

// =============================================================================
// __NAME__EditorModule — the game's editor module, compiled only into __NAME__Editor.exe. Adds the
// game's drawers / panels / resource types / menus to the generic editor.
// =============================================================================
class __NAME__EditorModule final : public Opaax::Editor::IEditorModule
{
public:
    void OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;
};
