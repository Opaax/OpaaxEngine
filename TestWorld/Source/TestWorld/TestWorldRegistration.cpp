// The TestWorld game's types. Each probe is a behaviour that records what the engine did in its
// own fields, where the scripts in TestWorld/Tests read it (expect.value). The ProbeLedger keeps a
// record across levels.
#include "Engine/Registries/AutoRegistration.h"
#include "Probes/AnimationProbes.h"
#include "Probes/CameraProbe.h"
#include "Probes/EventProbe.h"
#include "Probes/GameplayProbes.h"
#include "Probes/HierarchyProbes.h"
#include "Probes/LevelProbes.h"
#include "Probes/LifecycleProbes.h"
#include "Probes/MovementProbes.h"
#include "Probes/PhysicsProbes.h"
#include "Probes/ScriptingProbes.h"
#include "Probes/SpawnProbe.h"
#include "Probes/UIProbes.h"

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
    OPAAX_REGISTER_BEHAVIOUR(ControlsProbe);
    OPAAX_REGISTER_BEHAVIOUR(LaunchProbe);
    OPAAX_REGISTER_BEHAVIOUR(AudioProbe);
    OPAAX_REGISTER_BEHAVIOUR(AudioControlProbe);
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
    OPAAX_REGISTER_BEHAVIOUR(LiftProbe);
    OPAAX_REGISTER_BEHAVIOUR(SpeedProbe);
    OPAAX_REGISTER_BEHAVIOUR(LedgerProbe);
    OPAAX_REGISTER_BEHAVIOUR(TravelProbe);
    OPAAX_REGISTER_BEHAVIOUR(UIProbe);
    OPAAX_REGISTER_BEHAVIOUR(CameraProbe);
    OPAAX_REGISTER_BEHAVIOUR(WalkerProbe);
    OPAAX_REGISTER_BEHAVIOUR(ScriptProbe);
    OPAAX_REGISTER_BEHAVIOUR(HelperProbe);
    OPAAX_REGISTER_BEHAVIOUR(DieProbe);
    OPAAX_REGISTER_BEHAVIOUR(AnimProbe);

    OPAAX_REGISTER_GAME_INSTANCE_SUBSYSTEM(ProbeLedger);
}
