// The TestWorld game's types. Each probe is a behaviour that records what the engine did in its
// own fields, where the scripts in TestWorld/Tests read it (expect.value).
#include "Engine/Registries/AutoRegistration.h"
#include "Probes/EventProbe.h"
#include "Probes/GameplayProbes.h"
#include "Probes/HierarchyProbes.h"
#include "Probes/LifecycleProbes.h"
#include "Probes/PhysicsProbes.h"
#include "Probes/SpawnProbe.h"

namespace TestWorld
{
    OPAAX_REGISTER_BEHAVIOUR(LifecycleProbe);
    OPAAX_REGISTER_BEHAVIOUR(TimerProbe);
    OPAAX_REGISTER_BEHAVIOUR(SelfDestructProbe);
    OPAAX_REGISTER_BEHAVIOUR(EndProbe);
    OPAAX_REGISTER_BEHAVIOUR(WitnessProbe);
    OPAAX_REGISTER_BEHAVIOUR(EventProbe);
    OPAAX_REGISTER_BEHAVIOUR(SpawnProbe);
    OPAAX_REGISTER_BEHAVIOUR(InputProbe);
    OPAAX_REGISTER_BEHAVIOUR(LaunchProbe);
    OPAAX_REGISTER_BEHAVIOUR(AudioProbe);
    OPAAX_REGISTER_BEHAVIOUR(PoseProbe);
    OPAAX_REGISTER_BEHAVIOUR(BubbleProbe);
    OPAAX_REGISTER_BEHAVIOUR(ReparentProbe);
    OPAAX_REGISTER_BEHAVIOUR(MoveProbe);
    OPAAX_REGISTER_BEHAVIOUR(ContactProbe);
    OPAAX_REGISTER_BEHAVIOUR(BoundsProbe);
    OPAAX_REGISTER_BEHAVIOUR(ForceProbe);
    OPAAX_REGISTER_BEHAVIOUR(ImpulseProbe);
    OPAAX_REGISTER_BEHAVIOUR(SpinProbe);
    OPAAX_REGISTER_BEHAVIOUR(RayProbe);
}
