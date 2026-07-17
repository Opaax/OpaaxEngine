
#include "MyProject.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxTypes.h"

MyProject::MyProject(int InArgc, char** InArgv) : Opaax::CoreEngineApp(InArgc, InArgv)
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("Opaax Engine - MyProject Start");
    OPAAX_TRACE("==================================");
}

