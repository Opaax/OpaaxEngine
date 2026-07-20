#pragma once

#include "Editor/Extensions/IEditorModule.h"

// =============================================================================
// SandboxEditorModule — the Sandbox game's editor module (Editor.md D10). Compiled ONLY into
//   SandboxEditor.exe. Plugs Sandbox's editor extensions (drawers / panels / asset-types / menus /
//   edit-world-systems) into the generic editor. The editor analogue of SandboxModule::RegisterModule (D9):
//   the game registers INTO the editor's routes; the editor never knows Sandbox's types.
//   M0 is demonstrative (counts only) — real extensions land M2/M4/M5.
// =============================================================================
class SandboxEditorModule final : public Opaax::Editor::IEditorModule
{
public:
    void OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;
};
