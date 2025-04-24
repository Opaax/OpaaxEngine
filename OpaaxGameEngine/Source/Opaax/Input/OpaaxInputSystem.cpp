#include "OPpch.h"
#include "Opaax/Input/OpaaxInputSystem.h"

#include "Opaax/Log/OPLogMacro.h"

void OpaaxInputSystem::RegisterKeyboardInput(SDL_KeyboardEvent& lEvent)
{
    OPAAX_LOG("InputSystem::RegisterKeyboardInput %1%", %lEvent.raw)
}
