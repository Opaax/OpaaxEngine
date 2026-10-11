#include "SandboxApp.h"

SandboxApp::SandboxApp(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
}

void SandboxApp::OnInitializeApplication()
{
    // Intentionally empty: Sandbox registers no app-level config. Overridden so the base's
    // "not overridden" trace does not fire.
}

void SandboxApp::PostEngineStartup()
{
    // Intentionally empty: the world's content is data (the project's startupLevel), opened by
    // IEngine::FinishStartup before this runs. Overridden so the base's "not overridden" trace does
    // not fire.
}
