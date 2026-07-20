#pragma once

#include "Application/OpaaxApplication.h"

class SandboxApp : public Opaax::OpaaxApplication
{
public:
    SandboxApp(int InArgc, char** InArgv);
    
protected:
    void OnInitializeApplication() override;
    void RegisterModules(Opaax::ModuleRegistrar& InRegistrar) override;
    void PostEngineStartup() override;
};
