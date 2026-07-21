#pragma once

#include "Application/OpaaxApplication.h"

#ifdef OPAAX_PLATFORM_WINDOWS

extern Opaax::OpaaxApplication* CreateApplication(int InArgc, char** InArgv);

int main(int argc, char** argv)
{
    auto lApp = Opaax::UniquePtr<Opaax::OpaaxApplication>(CreateApplication(argc, argv));
    lApp->Bootstrap();
    lApp->InitializeApplication();
    lApp->RunApplication();
    lApp->ShutdownApplication();
    return 0;
}
#else
#error Opaax Game Engine only supports Windows!
#endif


// =============================================================================
// Helper macro to declare the application factory.
// =============================================================================
#define OPAAX_IMPLEMENT_APP(AppClass)                                       \
Opaax::OpaaxApplication* CreateApplication(int InArgc, char** InArgv)          \
{                                                                           \
    return new AppClass(InArgc, InArgv);                                    \
}