#include "SandboxApp.h"

#include "Application/Services/ILogger.h"
#include "Application/Services/IEngine.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "World/Entity/Entity.h"
#include "World/Components/DummyComponent.h"
#include "Application/Modules/ModuleRegistrar.h"
#include "Sandbox.h"

SandboxApp::SandboxApp(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
    //OPAAX_TRACE("-----------------------------------------------------");
    //OPAAX_TRACE("Opaax Application - Sandbox");
    //OPAAX_TRACE("-----------------------------------------------------");
}

void SandboxApp::OnInitializeApplication()
{
    // Intentionally empty: Sandbox registers no app-level config today (the MyConfig demo type is gone).
    // Kept as an override so the base's "not overridden" trace does not fire.
}

void SandboxApp::RegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    SandboxModule().OnRegister(InRegistrar);
}

void SandboxApp::PostEngineStartup()
{
    // Populate the active world via the shared module content (same scene as the editor host).
    if (Opaax::World* lWorld = GetAppService<Opaax::IEngine>().GetWorldManager().GetActiveWorld())
    {
        SandboxModule().SpawnDemoWorld(*lWorld);
    }
}
