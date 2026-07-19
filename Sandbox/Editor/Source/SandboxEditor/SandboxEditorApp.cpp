#include "SandboxEditorApp.h"

#include "Sandbox.h"
#include "Application/ModuleRegistrar.h"
#include "Application/Services/IEngine.h"
#include "World/WorldManager.h"
#include "World/World.h"

SandboxEditorApp::SandboxEditorApp(int InArgc, char** InArgv)
    : Opaax::Editor::EditorApplication(InArgc, InArgv)
{
}

void SandboxEditorApp::OnRegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    SandboxModule::RegisterModule(InRegistrar);
}

void SandboxEditorApp::PostEngineStartup()
{
    // Editor first — build the EditorContext (base), then populate the shared demo world.
    Opaax::Editor::EditorApplication::PostEngineStartup();

    if (Opaax::World* lWorld = GetAppService<Opaax::IEngine>().GetWorldManager().GetActiveWorld())
    {
        SandboxModule::SpawnDemoWorld(*lWorld);
    }
}
