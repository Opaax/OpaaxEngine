#pragma once

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"

OPAAX_LOG_CATEGORY(SandBox);

class SandboxApp : public Opaax::OpaaxApplication
{
public:
    SandboxApp(int InArgc, char** InArgv);
    
protected:
    void OnInitializeApplication() override;
    void RegisterModules(Opaax::ModuleRegistrar& InRegistrar) override;
    void PostEngineStartup() override;
};
