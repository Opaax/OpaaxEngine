// The TestWorld game's types. Each probe is a behaviour that records what the engine did in its
// own fields, where the scripts in TestWorld/Tests read it (expect.value). The ProbeLedger keeps a
// record across levels; the Extensions level uses a component, a data asset and a world subsystem of
// the game, and its own GameInstance class (ProbeGame, named in TestWorld.opaaxproj); the Saves level
// keeps the game's save (ProbeSave) between runs; the Settings level changes the player's settings;
// the Online level posts a run to the fake store and pauses on its overlay.
#include "Engine/Registries/AutoRegistration.h"
#include "Probes/AnimationProbes.h"
#include "Probes/CameraProbe.h"
#include "Probes/CommandProbes.h"
#include "Probes/EntityRefProbe.h"
#include "Probes/EventProbe.h"
#include "Probes/ExtensionProbes.h"
#include "Probes/FollowProbe.h"
#include "Probes/FrameProbe.h"
#include "Probes/GameplayProbes.h"
#include "Probes/HierarchyProbes.h"
#include "Probes/LevelProbes.h"
#include "Probes/LifecycleProbes.h"
#include "Probes/MovementProbes.h"
#include "Probes/OnlineProbes.h"
#include "Probes/PaintTextureProbes.h"
#include "Probes/PhysicsProbes.h"
#include "Probes/RuntimeTextureProbes.h"
#include "Probes/SaveProbes.h"
#include "Probes/ScriptingProbes.h"
#include "Probes/SettingsProbes.h"
#include "Probes/SpawnProbe.h"
#include "Probes/TimeScaleProbes.h"
#include "Probes/UIProbes.h"

namespace TestWorld
{
    // Only test maps carry the probes: the editor's Add Component menu leaves them out.
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(LifecycleProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(TimerProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(SelfDestructProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(EndProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(WitnessProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(EntityRefProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(EventProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(SpawnProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(InputProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ControlsProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(PointerProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(WheelProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(LaunchProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(AudioProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(AudioControlProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(StepsProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(BankProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(PoseProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(BubbleProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ReparentProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(MoveProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ContactProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(BoundsProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ForceProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ImpulseProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(SpinProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(RayProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(LiftProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(SpeedProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(LedgerProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(TravelProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(UIProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ScrollProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ModifierProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(LayerProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(CameraProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ShakeProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(FollowProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(FrameProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(DriftProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(WalkerProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(ScriptProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(HelperProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(DieProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(AnimProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(TuningProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(SaveProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(SettingsProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(RebindProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(LeaderboardProbe);
    OPAAX_REGISTER_UNLISTED_BEHAVIOUR(OverlayPauseProbe);

    OPAAX_REGISTER_OBJECT(ProbeLedger);

    // The game's own types beside the probes: a component, a data asset, a world subsystem and its
    // GameInstance class.
    OPAAX_REGISTER_COMPONENT(TunedComponent);
    OPAAX_REGISTER_OBJECT(ProbeTuning);
    OPAAX_REGISTER_OBJECT(ProbeCounterSubsystem);
    OPAAX_REGISTER_OBJECT(CommandProbeSubsystem);
    OPAAX_REGISTER_OBJECT(PaintTextureProbeSubsystem);
    OPAAX_REGISTER_OBJECT(RuntimeTextureProbeSubsystem);
    OPAAX_REGISTER_OBJECT(ProbeGame);

    // The game's save, which the Saves level keeps.
    OPAAX_REGISTER_OBJECT(ProbeSave);
}
