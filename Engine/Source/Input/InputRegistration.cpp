// The Input module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Input/Assets/InputActionResource.h"
#include "Input/Assets/InputMappingContextResource.h"
#include "Input/Mapping/InputMappingSubsystem.h"

namespace Opaax
{
    // Gameplay binds to actions; a mapping context says which keys trigger them.
    OPAAX_REGISTER_NAMED_RESOURCE(InputActionResource, "InputAction");
    OPAAX_REGISTER_NAMED_RESOURCE(InputMappingContextResource, "InputMappingContext");

    // After the UI subsystem (order 100): the UI consumes the input it used first.
    OPAAX_REGISTER_NAMED_GAME_INSTANCE_SUBSYSTEM(InputMappingSubsystem, "InputMapping", 200);
}
