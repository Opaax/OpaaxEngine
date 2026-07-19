#include "SandboxApp.h"

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

void SandboxApp::OnRegisterModules(Opaax::ModuleRegistrar& InRegistrar)
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
