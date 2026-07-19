#pragma once

#include "Core/Application/OpaaxApplication.h"

class SandboxApp : public Opaax::OpaaxApplication
{
public:
    SandboxApp(int InArgc, char** InArgv);
    
protected:
    void OnInitializeApplication() override;
    void OnRegisterModules(Opaax::ModuleRegistrar& InRegistrar) override;
    void PostEngineStartup() override;
};
