#pragma once
#include <SDL3/SDL_events.h>
#include "Opaax/OpaaxNonCopyableAndMovable.h"

class OPAAX_API OpaaxInputSystem : public OpaaxNonCopyableAndMovable
{
public:
    OpaaxInputSystem() = default;

    void RegisterKeyboardInput(SDL_KeyboardEvent& lEvent);
};
