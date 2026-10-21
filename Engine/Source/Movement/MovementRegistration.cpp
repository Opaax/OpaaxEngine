// The Movement module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Movement/Assets/MoveMode.h"
#include "Movement/Assets/Mover.h"
#include "Movement/Modes/FlyMoveMode.h"
#include "Movement/Modes/GroundMoveMode.h"
#include "Movement/MoverComponent.h"
#include "Movement/MoverSubsystem.h"

namespace Opaax
{
    OPAAX_REGISTER_NAMED_COMPONENT(MoverComponent, "Mover");

    // A move mode asset is one way of moving and its tuning (like an animation clip); a Mover names
    // several (like a library). The class names are saved in the files.
    OPAAX_REGISTER_OBJECT(MoveMode);
    OPAAX_REGISTER_OBJECT(GroundMoveMode);
    OPAAX_REGISTER_OBJECT(FlyMoveMode);
    OPAAX_REGISTER_OBJECT(Mover);

    // After Physics: the mover must see this step's poses.
    OPAAX_REGISTER_OBJECT(MoverSubsystem);
}
