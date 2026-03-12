#pragma once

#ifdef OPAAX_PLATFORM_WINDOWS
    extern Opaax::CoreEngineApp* CreateApplication();

int main(int argc, char** argv)
{
    auto lApp = CreateApplication();
    lApp->Run();
    lApp->Shutdown();
    delete lApp;
    return 0;
}
#else
#error Opaax Game Engine only supports Windows!
#endif


// ==================================
// Macro pour simplifier la déclaration
// ==================================
#define OPAAX_IMPLEMENT_APP(AppClass)           \
Opaax::CoreEngineApp* CreateApplication()         \
{                                           \
    return new AppClass();                  \
}