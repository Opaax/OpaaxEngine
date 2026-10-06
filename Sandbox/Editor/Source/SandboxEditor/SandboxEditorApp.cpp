#include "SandboxEditorApp.h"

#include "SandboxEditorModule.h"
#include "Sandbox.h"
#include "Engine/Registries/ModuleRegistrar.h"

SandboxEditorApp::SandboxEditorApp(int InArgc, char** InArgv)
    : Opaax::Editor::EditorApplication(InArgc, InArgv)
{
}

void SandboxEditorApp::RegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    SandboxModule().OnRegister(InRegistrar);
}

void SandboxEditorApp::OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // Sandbox's editor extensions plug in here: after the game module, before the first world.
    SandboxEditorModule().OnRegister(InRegistrar);
}

void SandboxEditorApp::PostEngineStartup()
{
    // Just the base (which builds the EditorContext). The world's content comes from
    // Assets/Levels/Main.opaaxlevel, the same file Sandbox.exe opens.
    Opaax::Editor::EditorApplication::PostEngineStartup();
}
