#pragma once

#include "Application/OpaaxApplication.h"

// =============================================================================
// The program entry point. A host executable includes this header once and names its
// application class with OPAAX_IMPLEMENT_APP.
// =============================================================================

extern Opaax::OpaaxApplication* CreateApplication(int InArgc, char** InArgv);

int main(int argc, char** argv)
{
    auto lApp = Opaax::TUniquePtr<Opaax::OpaaxApplication>(CreateApplication(argc, argv));
    lApp->Bootstrap();
    lApp->InitializeApplication();
    lApp->RunApplication();
    lApp->ShutdownApplication();
    return lApp->GetExitCode();
}

// =============================================================================
// Declares the application factory.
// =============================================================================
#define OPAAX_IMPLEMENT_APP(AppClass)                                       \
Opaax::OpaaxApplication* CreateApplication(int InArgc, char** InArgv)       \
{                                                                           \
    return new AppClass(InArgc, InArgv);                                    \
}
