#include <Sandbox.h>

#include "Core/Application/Services/IConfigSystem.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IEngine.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/World/WorldManager.h"
#include "Core/World/World.h"
#include "Entity/Entity.h"
#include "Core/Components/DummyComponent.h"
#include "Core/Application/ModuleRegistrar.h"
#include "Config/ConfigTest.h"
#include "SandboxModule.h"

Sandbox::Sandbox(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
    //OPAAX_TRACE("-----------------------------------------------------");
    //OPAAX_TRACE("Opaax Application - Sandbox");
    //OPAAX_TRACE("-----------------------------------------------------");
}

void Sandbox::OnInitializeApplication()
{
    GetAppService<Opaax::ILogger>().Critical(Opaax::OpaaxString("Call from logger service"));
    GetAppService<Opaax::IConfigSystem>().Register<Config_MyConfig>();
}

void Sandbox::OnRegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    // D9 — the app routes its game module through the single RegisterModule() entry point. The
    // editor host (S8) will call the exact same function through its own OnRegisterModules.
    SandboxModule::RegisterModule(InRegistrar);
}

void Sandbox::PostEngineStartup()
{
    // Populate the active world via the shared module content (same scene as the editor host).
    if (Opaax::World* lWorld = GetAppService<Opaax::IEngine>().GetWorldManager().GetActiveWorld())
    {
        SandboxModule::SpawnDemoWorld(*lWorld);
    }
}
