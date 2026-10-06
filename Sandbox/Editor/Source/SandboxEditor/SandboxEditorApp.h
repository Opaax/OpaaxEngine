#pragma once

#include "Editor/Application/EditorApplication.h"

// =============================================================================
// SandboxEditorApp — the game's editor executable: a thin layer over the generic OpaaxEditorLib.
// The game's types register themselves (same module as Sandbox.exe). The editor is generic; the
// game registers into it, never the reverse.
// =============================================================================
class SandboxEditorApp final : public Opaax::Editor::EditorApplication
{
public:
    SandboxEditorApp(int InArgc, char** InArgv);

protected:
    void OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override; 
    void PostEngineStartup() override;
    
    Opaax::OpaaxString GetEditedProjectName() const override { return Opaax::OpaaxString("Sandbox"); }
};
