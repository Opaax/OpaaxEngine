#include "TestWorldApp.h"

TestWorldApp::TestWorldApp(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
}

void TestWorldApp::OnInitializeApplication()
{
    // Intentionally empty: TestWorld registers no app-level config. Overridden so the base's
    // "not overridden" trace does not fire.
}
