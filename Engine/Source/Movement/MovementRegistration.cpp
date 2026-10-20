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

    // A MoveMode is one tuning (like an animation clip); a Mover names several (like a library).
    OPAAX_REGISTER_OBJECT(MoveMode);
    OPAAX_REGISTER_OBJECT(Mover);

    // These names are saved in MoveMode assets: renaming one breaks existing assets.
    OPAAX_REGISTER_MOVER_MODE(GroundMoveMode, "GroundMove");
    OPAAX_REGISTER_MOVER_MODE(FlyMoveMode, "FlyMove");

    // After Physics: the mover must see this step's poses.
    OPAAX_REGISTER_OBJECT(MoverSubsystem);
}
