#include "SandboxApp.h"

#include "Engine/Registries/ModuleRegistrar.h"
#include "Sandbox.h"

SandboxApp::SandboxApp(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
    //OPAAX_TRACE("-----------------------------------------------------");
    //OPAAX_TRACE("Opaax Application - Sandbox");
    //OPAAX_TRACE("-----------------------------------------------------");
}

void SandboxApp::OnInitializeApplication()
{
    // Intentionally empty: Sandbox registers no app-level config. Overridden so the base's
    // "not overridden" trace does not fire.
}

void SandboxApp::RegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    SandboxModule().OnRegister(InRegistrar);
}

void SandboxApp::PostEngineStartup()
{
    // Intentionally empty: the world's content is data (the project's startupLevel), opened by
    // IEngine::FinishStartup before this runs. Overridden so the base's "not overridden" trace does
    // not fire.
}
