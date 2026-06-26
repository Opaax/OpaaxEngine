#include <Sandbox.h>
#include "Core/Log/OpaaxLog.h"

Sandbox::Sandbox(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
    OPAAX_TRACE("-----------------------------------------------------");
    OPAAX_TRACE("Opaax Application - Sandbox");
    OPAAX_TRACE("-----------------------------------------------------");
}
