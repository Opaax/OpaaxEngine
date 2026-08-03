#include "SandboxApp.h"

#include "Engine/Modules/ModuleRegistrar.h"
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
    // Intentionally empty since M5. The world's content is DATA now: the project's startupLevel
    // names Levels/Main.opaaxlevel, and IEngine::FinishStartup opened it before this ran. The
    // three quads that used to be spawned here by C++ are Sandbox/Assets/Maps/Main.opaaxmap, and
    // both hosts read the same file — which is the whole point of the milestone.
    //
    // Kept as an override so the base's "not overridden" trace does not fire.
}
