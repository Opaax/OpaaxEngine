#pragma once

#include "Editor/EditorApplication.h"

// =============================================================================
// SandboxEditorApp — the game's editor executable (Editor.md D8): a thin composition of the generic
// OpaaxEditorLib. It registers the SAME SandboxModule as Sandbox.exe (D9) and populates the same demo
// world. The editor is generic; the game registers into it — never the reverse.
// =============================================================================
class SandboxEditorApp final : public Opaax::Editor::EditorApplication
{
public:
    SandboxEditorApp(int InArgc, char** InArgv);

protected:
    void RegisterModules(Opaax::ModuleRegistrar& InRegistrar) override;
    void OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override; 
    void PostEngineStartup() override;
    
    Opaax::OpaaxString GetEditedProjectName() const override { return Opaax::OpaaxString("Sandbox"); }
};
