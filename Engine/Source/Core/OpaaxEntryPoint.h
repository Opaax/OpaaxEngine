#pragma once

#ifdef OPAAX_PLATFORM_WINDOWS
    extern Opaax::CoreEngineApp* CreateApplication(int InArgc, char** InArgv);

int main(int argc, char** argv)
{
    auto lApp = Opaax::UniquePtr<Opaax::CoreEngineApp>(CreateApplication(argc, argv));
    lApp->Run();
    return 0;
}
#else
#error Opaax Game Engine only supports Windows!
#endif


// =============================================================================
// Helper macro to declare the application factory.
// =============================================================================
#define OPAAX_IMPLEMENT_APP(AppClass)                                       \
Opaax::CoreEngineApp* CreateApplication(int InArgc, char** InArgv)          \
{                                                                           \
    return new AppClass(InArgc, InArgv);                                    \
}