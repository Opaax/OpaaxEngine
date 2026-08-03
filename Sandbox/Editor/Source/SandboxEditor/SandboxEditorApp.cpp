#include "SandboxEditorApp.h"

#include "SandboxEditorModule.h"
#include "Sandbox.h"
#include "Engine/Modules/ModuleRegistrar.h"

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
    // D10: Sandbox's editor extensions plug in here — after the game module, before the first world.
    SandboxEditorModule().OnRegister(InRegistrar);
}

void SandboxEditorApp::PostEngineStartup()
{
    // Just the base (which builds the EditorContext). Since M5 there is nothing to populate: the
    // world's content came from Assets/Levels/Main.opaaxlevel during FinishStartup, the same file
    // Sandbox.exe opens. The editor host used to call SpawnDemoWorld here so the two hosts would
    // show the same scene — they now do so by reading the same map, which is a much stronger
    // version of the same claim.
    Opaax::Editor::EditorApplication::PostEngineStartup();
}
