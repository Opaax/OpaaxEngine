#include <Sandbox.h>

#include "Core/Application/Services/IConfigSystem.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Log/OpaaxLog.h"
#include "Config/ConfigTest.h"

Sandbox::Sandbox(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
    //OPAAX_TRACE("-----------------------------------------------------");
    //OPAAX_TRACE("Opaax Application - Sandbox");
    //OPAAX_TRACE("-----------------------------------------------------");
}

void Sandbox::OnInitializeApplication()
{
    GetAppService<Opaax::ILogger>().Critical(Opaax::OpaaxString("Call from logger service"));
    GetAppService<Opaax::IConfigSystem>().Register<Config_MyConfig>();
}
