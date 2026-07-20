#include "SandboxApp.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IEngine.h"
#include "Core/Log/OpaaxLog.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "World/Entity/Entity.h"
#include "World/Components/DummyComponent.h"
#include "Application/ModuleRegistrar.h"
#include "Config/ConfigTest.h"
#include "Sandbox.h"

SandboxApp::SandboxApp(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
    //OPAAX_TRACE("-----------------------------------------------------");
    //OPAAX_TRACE("Opaax Application - Sandbox");
    //OPAAX_TRACE("-----------------------------------------------------");
}

void SandboxApp::OnInitializeApplication()
{
    GetAppService<Opaax::IConfigSystem>().Register<Config_MyConfig>();
}

void SandboxApp::RegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    SandboxModule::RegisterModule(InRegistrar);
}

void SandboxApp::PostEngineStartup()
{
    // Populate the active world via the shared module content (same scene as the editor host).
    if (Opaax::World* lWorld = GetAppService<Opaax::IEngine>().GetWorldManager().GetActiveWorld())
    {
        SandboxModule::SpawnDemoWorld(*lWorld);
    }
}
