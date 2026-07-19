#include "SandboxEditorApp.h"

#include "SandboxModule.h"
#include "Core/Application/ModuleRegistrar.h"
#include "Core/Application/Services/IEngine.h"
#include "Core/World/WorldManager.h"
#include "Core/World/World.h"

SandboxEditorApp::SandboxEditorApp(int InArgc, char** InArgv)
    : Opaax::Editor::EditorApplication(InArgc, InArgv)
{
}

void SandboxEditorApp::OnRegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    // D9 — the SAME entry point Sandbox.exe uses. The editor host adds nothing the runtime lacks.
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
