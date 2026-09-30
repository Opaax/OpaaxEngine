#pragma once

#include "Editor/Extensions/IEditorModule.h"

// =============================================================================
// SandboxEditorModule — the Sandbox game's editor module, compiled only into SandboxEditor.exe.
//   Adds Sandbox's editor extensions (drawers, menus, resource types...) to the generic editor.
//   The editor never knows Sandbox's types.
// =============================================================================
class SandboxEditorModule final : public Opaax::Editor::IEditorModule
{
public:
    void OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;
};
