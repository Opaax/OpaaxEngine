// Suite: CameraRigSubsystem — a camera's stages run in each fixed step by stage (Body, Size, then
// Constraint) whatever order they were added in, each on the pose the ones before it left, and the
// result is written to the camera once, in the world: a rigged camera moves in the steps only, so
// its rhythm is "steps" at any frames. Driven through WorldManager the way Engine::Loop runs a frame.
// CameraFollowStage: a dead zone, damping per axis and a look-ahead, the same over one step or two halves.
// CameraConfinerStage: the whole view kept inside its bounds, its size, the aspect and its roll counted.
// CameraGroupStage: several entities framed with their padding at the aspect, the size clamped.
// CameraZoneStage: the zone holding the target sets the size and bounds, blended in without a jump.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraConfinerStage.h"
#include "Renderer/Camera/CameraFollowStage.h"
#include "Renderer/Camera/CameraGroupStage.h"
#include "Renderer/Camera/CameraZoneComponent.h"
#include "Renderer/Camera/CameraZoneStage.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/CameraRhythm.h"
#include "Renderer/Camera/CameraRigSubsystem.h"
#include "Renderer/Camera/CameraStage.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    constexpr double STEP_SECONDS = 1.0 / 60.0;

    /** The stages that ran, in order. */
    TDynArray<ECameraStage>& Ran()
    {
        static TDynArray<ECameraStage> s_Ran;
        return s_Ran;
    }
}

// A game's namespace, as a game module would declare its stages.
namespace RigProbes
{
    /** Moves the camera right at Speed. */
    class PushStage final : public CameraStage
    {
        OPAAX_OBJECT(PushStage, CameraStage)

    public:
        float Speed = 1800.f;

        ECameraStage GetStage() const override { return ECameraStage::Body; }

        void Step(CameraRigPose& InOutPose, const float InDeltaTime) override
        {
            Ran().push_back(GetStage());
            InOutPose.Position.x += Speed * InDeltaTime;
        }
    };

    /** Frames 200 units more than how far right the camera is: run after PushStage, it sees the push. */
    class WidenStage final : public CameraStage
    {
        OPAAX_OBJECT(WidenStage, CameraStage)

    public:
        ECameraStage GetStage() const override { return ECameraStage::Size; }

        void Step(CameraRigPose& InOutPose, float) override
        {
            Ran().push_back(GetStage());
            InOutPose.OrthoSize = 200.f + InOutPose.Position.x;
        }
    };

    /** Keeps the camera left of Limit: run last, it has the last word. */
    class WallStage final : public CameraStage
    {
        OPAAX_OBJECT(WallStage, CameraStage)

    public:
        float Limit = 20.f;

        ECameraStage GetStage() const override { return ECameraStage::Constraint; }

        void Step(CameraRigPose& InOutPose, float) override
        {
            Ran().push_back(GetStage());
            InOutPose.Position.x = Maths::Min(InOutPose.Position.x, Limit);
        }
    };
}

namespace
{
    using namespace RigProbes;

    // A Play world with behaviours and the rig: the stage types registered constraint first.
    struct RigWorld
    {
        ResourceManager   Resources;
        EngineEventBus    Events;
        DebugDraw         Debug;
        EngineConfigData  Config;
        InputManager      Input;
        ComponentRegistry Components;
        WorldManager      Worlds;   // last: its worlds end before the services they use

        World* TheWorld = nullptr;

        RigWorld()
        {
            Ran().clear();
            REQUIRE(Components.Register<TransformComponent>(OpaaxStringID("Transform"), /*bEssential*/true));
            REQUIRE(Components.Register<WallStage>(OpaaxStringID("WallStage")));
            REQUIRE(Components.Register<WidenStage>(OpaaxStringID("WidenStage")));
            REQUIRE(Components.Register<PushStage>(OpaaxStringID("PushStage")));
            REQUIRE(Components.Register<CameraFollowStage>(OpaaxStringID("CameraFollowStage")));
            REQUIRE(Components.Register<CameraConfinerStage>(OpaaxStringID("CameraConfinerStage")));
            REQUIRE(Components.Register<CameraGroupStage>(OpaaxStringID("CameraGroupStage")));
            REQUIRE(Components.Register<CameraZoneStage>(OpaaxStringID("CameraZoneStage")));

            TheWorld = Worlds.CreateWorld("Rig", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);
            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events, Input, Config,
                                               /*Actions*/ nullptr, /*UI*/ nullptr, Debug, &Components });
            TheWorld->GetSubsystems().Add<BehaviourSubsystem>();
            TheWorld->GetSubsystems().Add<CameraRigSubsystem>();
            TheWorld->GetSubsystems().StartupAll();
            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        Entity MakeCamera(const char* InName)
        {
            Entity lCamera = TheWorld->CreateEntity(InName);
            lCamera.Add<CameraComponent>();
            return lCamera;
        }

        /** Update, the camera manager's update, then InSteps steps. */
        void Tick(const Uint32 InSteps)
        {
            Worlds.Update(STEP_SECONDS);
            CameraManager::RecordCameraSteps(*TheWorld);

            for (Uint32 lStep = 0; lStep < InSteps; ++lStep)
            {
                Worlds.FixedUpdate(STEP_SECONDS);
            }
        }
    };
}

TEST_CASE("CameraRigSubsystem: a camera's stages run by stage, whatever order they were added in")
{
    RigWorld lRig;
    Entity   lCamera = lRig.MakeCamera("Rigged");
    lCamera.Add<WallStage>();
    lCamera.Add<WidenStage>();
    lCamera.Add<PushStage>();

    lRig.Tick(1);

    REQUIRE(Ran().size() == 3);
    CHECK(Ran()[0] == ECameraStage::Body);
    CHECK(Ran()[1] == ECameraStage::Size);
    CHECK(Ran()[2] == ECameraStage::Constraint);

    // Pushed 30, widened by the push, then walled at 20.
    CHECK(lCamera.Get<TransformComponent>().Position.x == doctest::Approx(20.f));
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(230.f));

    // Each step goes on from where the last left the camera: pushed to 50, walled again.
    lRig.Tick(1);
    CHECK(lCamera.Get<TransformComponent>().Position.x == doctest::Approx(20.f));
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(250.f));
}

TEST_CASE("CameraRigSubsystem: stages place a child camera in the world; a size is kept in range; no Camera, no rig")
{
    RigWorld lRig;
    Entity   lCarrier = lRig.TheWorld->CreateEntity("Carrier");
    lCarrier.Get<TransformComponent>().Position = { 100.f, 0.f };

    Entity lCamera = lRig.MakeCamera("Carried");
    REQUIRE(EntityHierarchy::SetParent(lCamera, lCarrier, /*bInKeepWorld*/false));
    lCamera.Add<PushStage>();
    lCamera.Add<WidenStage>();

    Entity lLoose = lRig.TheWorld->CreateEntity("NotACamera");
    lLoose.Add<PushStage>();

    lRig.Tick(1);

    CHECK(EntityHierarchy::WorldTransform(lCamera).Position.x == doctest::Approx(130.f));
    CHECK(lCamera.Get<TransformComponent>().Position.x == doctest::Approx(30.f));
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(330.f));
    CHECK(lLoose.Get<TransformComponent>().Position.x == 0.f);

    lCamera.Get<PushStage>().Speed = -1.e7f;
    lRig.Tick(1);
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == CameraComponent::MIN_ORTHO_SIZE);
}

TEST_CASE("CameraRigSubsystem: a rigged camera moves in the steps only, at any frames")
{
    // Steps per frame, as a clock gives them.
    constexpr Uint32 FRAMES[] = { 1, 0, 0, 1, 2, 3, 0, 1, 2, 1 };

    RigWorld lRig;
    Entity   lCamera = lRig.MakeCamera("Rigged");
    lCamera.Add<PushStage>().Speed = 120.f;
    lCamera.Add<WidenStage>();

    Uint64 lFrame = 0;
    while (lRig.TheWorld->GetStepCount() < 2 * CameraRhythm::WINDOW_STEPS + 1)
    {
        lRig.Tick(FRAMES[lFrame++ % (sizeof(FRAMES) / sizeof(FRAMES[0]))]);
    }

    const CameraRhythm& lRhythm = lCamera.Get<CameraInterpolationComponent>().Rhythm;
    CHECK(lRhythm.Last == ECameraRhythm::Steps);
    CHECK_FALSE(lRhythm.bWarned);

    // Where the steps put it, whatever the frames: 2 units a step.
    const float lSteps = static_cast<float>(lRig.TheWorld->GetStepCount());
    CHECK(lCamera.Get<TransformComponent>().Position.x == doctest::Approx(2.f * lSteps).epsilon(1e-4));
}

// =============================================================================
// Follow
// =============================================================================
namespace
{
    /** Half a 800 by 600 view. */
    const Vector2F HALF_VIEW{ 400.f, 300.f };

    constexpr float DT = static_cast<float>(STEP_SECONDS);

    void CheckNear(const Vector2F InActual, const Vector2F InExpected, const double InEpsilon = 1e-5)
    {
        CHECK(InActual.x == doctest::Approx(InExpected.x).epsilon(InEpsilon));
        CHECK(InActual.y == doctest::Approx(InExpected.y).epsilon(InEpsilon));
    }
}

TEST_CASE("StepCameraFollow: holds while the aim is in the dead zone, follows by the overshoot outside it")
{
    CameraFollowSettings lSettings;
    lSettings.DeadZone = { 0.5f, 0.5f };   // 200 by 150 each side of the camera

    CameraFollowState lState;
    Vector2F          lCamera{ 0.f, 0.f };

    lCamera = StepCameraFollow(lCamera, { 150.f, -100.f }, HALF_VIEW, lSettings, lState, DT);
    CheckNear(lCamera, { 0.f, 0.f });

    lCamera = StepCameraFollow(lCamera, { 250.f, -100.f }, HALF_VIEW, lSettings, lState, DT);
    CheckNear(lCamera, { 50.f, 0.f });

    lCamera = StepCameraFollow(lCamera, { 250.f, -200.f }, HALF_VIEW, lSettings, lState, DT);
    CheckNear(lCamera, { 50.f, -50.f });

    // The offset moves the aim: the camera holds 40 under a target 40 too low.
    lSettings          = CameraFollowSettings{};
    lSettings.Offset   = { 0.f, 40.f };
    lCamera            = StepCameraFollow(lCamera, { 10.f, -80.f }, HALF_VIEW, lSettings, lState, DT);
    CheckNear(lCamera, { 10.f, -40.f });
}

TEST_CASE("StepCameraFollow: damped, a step of dt is two of dt / 2")
{
    CameraFollowSettings lSettings;
    lSettings.DeadZone = { 0.2f, 0.1f };
    lSettings.Damping  = { 0.3f, 0.6f };

    // A target far outside the dead zone, already followed for a step: the camera carries a velocity.
    CameraFollowState lState;
    const Vector2F    lCamera = StepCameraFollow({ 0.f, 0.f }, { 300.f, 200.f }, HALF_VIEW, lSettings, lState, DT);
    CHECK(lCamera.x > 0.f);
    CHECK(lCamera.x < 300.f - 80.f);

    CameraFollowState lOneState = lState;
    const Vector2F    lOne      = StepCameraFollow(lCamera, { 300.f, 200.f }, HALF_VIEW, lSettings, lOneState, DT);

    CameraFollowState lTwoState = lState;
    Vector2F          lTwo      = StepCameraFollow(lCamera, { 300.f, 200.f }, HALF_VIEW, lSettings, lTwoState, DT / 2.f);
    lTwo                        = StepCameraFollow(lTwo, { 300.f, 200.f }, HALF_VIEW, lSettings, lTwoState, DT / 2.f);

    CheckNear(lTwo, lOne);
    CheckNear(lTwoState.Velocity, lOneState.Velocity, 1e-4);
}

TEST_CASE("StepCameraFollow: the aim leads the target by its velocity over the steps times LookAhead")
{
    CameraFollowSettings lSettings;
    lSettings.LookAhead = 0.5f;

    // 2 units a step is 120 a second: led by 60. None on the first step, nothing to measure yet.
    CameraFollowState lState;
    Vector2F          lCamera = StepCameraFollow({ 0.f, 0.f }, { 0.f, 0.f }, HALF_VIEW, lSettings, lState, DT);
    CheckNear(lCamera, { 0.f, 0.f });
    lCamera = StepCameraFollow(lCamera, { 2.f, 0.f }, HALF_VIEW, lSettings, lState, DT);
    CheckNear(lCamera, { 62.f, 0.f }, 1e-4);

    // Eased in, the lead is the same over one step or two halves of it, the target running on.
    lSettings.LookAheadDamping = 0.2f;
    CameraFollowState lOneState = lState;
    CameraFollowState lTwoState = lState;
    lOneState.Lead              = { 0.f, 0.f };
    lTwoState.Lead              = { 0.f, 0.f };
    StepCameraFollow(lCamera, { 4.f, 0.f }, HALF_VIEW, lSettings, lOneState, DT);
    StepCameraFollow(lCamera, { 3.f, 0.f }, HALF_VIEW, lSettings, lTwoState, DT / 2.f);
    StepCameraFollow(lCamera, { 4.f, 0.f }, HALF_VIEW, lSettings, lTwoState, DT / 2.f);
    CHECK(lOneState.Lead.x > 0.f);
    CHECK(lOneState.Lead.x < 60.f);
    CheckNear(lTwoState.Lead, lOneState.Lead, 1e-4);
}

TEST_CASE("CameraFollowStage: locked on its target without damping or a dead zone; the dead zone counts the aspect")
{
    RigWorld lRig;
    Entity   lRunner = lRig.TheWorld->CreateEntity("Runner");

    Entity             lLocked = lRig.MakeCamera("Locked");
    CameraFollowStage& lLock   = lLocked.Add<CameraFollowStage>();
    lLock.Target               = "Runner";
    lLock.Offset               = { 0.f, 40.f };

    Entity lWide = lRig.MakeCamera("Wide");
    lWide.Get<CameraComponent>().OrthoSize = 100.f;
    CameraFollowStage& lZone = lWide.Add<CameraFollowStage>();
    lZone.Target             = "Runner";
    lZone.DeadZone           = { 0.5f, 0.f };

    // Drawn 2 wide for 1 tall: the dead zone is 100 each side, where 16:9 would make it 89.
    lRig.TheWorld->SetViewAspect(2.f);

    for (const float lX : { 30.f, 150.f, 400.f })
    {
        lRunner.Get<TransformComponent>().Position = { lX, -12.f };
        lRig.Tick(1);
        CheckNear(lLocked.Get<TransformComponent>().Position, { lX, 28.f });
    }
    CheckNear(lWide.Get<TransformComponent>().Position, { 300.f, -12.f });

    // Gone, it holds; another of its name is followed.
    lRunner.Destroy();
    lRig.Tick(1);
    CheckNear(lLocked.Get<TransformComponent>().Position, { 400.f, 28.f });
    Entity lNext = lRig.TheWorld->CreateEntity("Runner");
    lNext.Get<TransformComponent>().Position = { -50.f, 0.f };
    lRig.Tick(1);
    CheckNear(lLocked.Get<TransformComponent>().Position, { -50.f, 40.f });
}

// =============================================================================
// Confiner
// =============================================================================
TEST_CASE("ConfineCameraView: the view kept inside, centred when wider than the bounds; an axis of no extent is free")
{
    const Bounds2D ROOM = Bounds2D::FromCenterSize({ 100.f, 0.f }, { 1000.f, 600.f });   // x -400..600, y -300..300

    // A view 200 by 100 each side moves within x -200..400, y -200..200.
    CheckNear(ConfineCameraView({ 50.f, 10.f }, { 200.f, 100.f }, ROOM), { 50.f, 10.f });
    CheckNear(ConfineCameraView({ 900.f, -250.f }, { 200.f, 100.f }, ROOM), { 400.f, -200.f });
    CheckNear(ConfineCameraView({ -900.f, 250.f }, { 200.f, 100.f }, ROOM), { -200.f, 200.f });

    // Wider than the room on x only: centred on x, still kept on y.
    CheckNear(ConfineCameraView({ 900.f, 250.f }, { 600.f, 100.f }, ROOM), { 100.f, 200.f });

    // No height: y is free.
    const Bounds2D WALLS = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 1000.f, 0.f });
    CheckNear(ConfineCameraView({ 900.f, 5000.f }, { 200.f, 100.f }, WALLS), { 300.f, 5000.f });
}

TEST_CASE("CameraConfinerStage: after the body stages; the size, the aspect and a roll counted; placed on an entity")
{
    RigWorld lRig;
    lRig.TheWorld->SetViewAspect(2.f);

    // OrthoSize 100 at aspect 2: 200 by 100 each side, so x stops at 500 - 200.
    Entity lCamera = lRig.MakeCamera("Walled");
    lCamera.Get<CameraComponent>().OrthoSize = 100.f;
    CameraConfinerStage& lConfiner = lCamera.Add<CameraConfinerStage>();
    lConfiner.Size                 = { 1000.f, 600.f };
    lCamera.Add<PushStage>().Speed = 6000.f;   // 100 a step, added after the confiner

    for (Int32 lStep = 0; lStep < 5; ++lStep)
    {
        lRig.Tick(1);
    }
    CheckNear(lCamera.Get<TransformComponent>().Position, { 300.f, 0.f });

    // Zoomed out past the room's width: centred on it.
    lCamera.Get<CameraComponent>().OrthoSize = 300.f;
    lRig.Tick(1);
    CheckNear(lCamera.Get<TransformComponent>().Position, { 0.f, 0.f });

    // Rolled a quarter turn, it reaches 100 sideways and 200 up: x stops at 400.
    lCamera.Get<CameraComponent>().OrthoSize   = 100.f;
    lCamera.Get<CameraComponent>().bRotateView = true;
    lCamera.Get<TransformComponent>().Rotation = 90.f;
    for (Int32 lStep = 0; lStep < 5; ++lStep)
    {
        lRig.Tick(1);
    }
    CHECK(lCamera.Get<TransformComponent>().Position.x == doctest::Approx(400.f).epsilon(1e-4));

    // On an entity: moved to 1000 and doubled, the room is 2000 wide from 1000: x stops at 1800.
    Entity lRoom = lRig.TheWorld->CreateEntity("Room");
    lRoom.Get<TransformComponent>().Position = { 1000.f, 0.f };
    lRoom.Get<TransformComponent>().Scale    = { 2.f, 2.f };
    lConfiner.BoundsEntity                   = "Room";
    lCamera.Get<CameraComponent>().bRotateView = false;
    for (Int32 lStep = 0; lStep < 20; ++lStep)
    {
        lRig.Tick(1);
    }
    CHECK(lCamera.Get<TransformComponent>().Position.x == doctest::Approx(1800.f));

    // Its entity gone, it confines nothing until another of its name comes.
    lRoom.Destroy();
    lRig.Tick(1);
    CHECK(lCamera.Get<TransformComponent>().Position.x == doctest::Approx(1900.f));
}

// =============================================================================
// Group
// =============================================================================
TEST_CASE("FrameCameraGroup: two targets apart fit with their padding at the aspect; the size is clamped")
{
    constexpr float WIDE = 16.f / 9.f;

    // Side by side: the width decides. 300 + 50 each side of the centre, so 350 / (16 / 9) tall.
    const CameraGroupFrame lApart = FrameCameraGroup(Bounds2D::FromMinMax({ -300.f, 0.f }, { 300.f, 100.f }), 50.f,
                                                     WIDE, 10.f, 1000.f);
    CheckNear(lApart.Center, { 0.f, 50.f });
    CHECK(lApart.OrthoSize == doctest::Approx(196.875f));
    CHECK(lApart.OrthoSize * WIDE == doctest::Approx(350.f));

    // One above the other: the height decides.
    const CameraGroupFrame lTall = FrameCameraGroup(Bounds2D::FromMinMax({ 0.f, -400.f }, { 10.f, 400.f }), 50.f,
                                                    WIDE, 10.f, 1000.f);
    CHECK(lTall.OrthoSize == doctest::Approx(450.f));

    // Close together it zooms in no closer than MinSize; far apart, out no further than MaxSize.
    CHECK(FrameCameraGroup(Bounds2D::FromMinMax({ 0.f, 0.f }, { 1.f, 1.f }), 0.f, WIDE, 120.f, 1000.f).OrthoSize
          == doctest::Approx(120.f));
    CHECK(FrameCameraGroup(Bounds2D::FromMinMax({ -5000.f, 0.f }, { 5000.f, 0.f }), 50.f, WIDE, 120.f, 1000.f).OrthoSize
          == doctest::Approx(1000.f));
}

TEST_CASE("CameraGroupStage: frames the targets found as they move; one gone is left out; eased over Damping")
{
    RigWorld lRig;
    lRig.TheWorld->SetViewAspect(2.f);

    Entity lLeft  = lRig.TheWorld->CreateEntity("Left");
    Entity lRight = lRig.TheWorld->CreateEntity("Right");
    lLeft.Get<TransformComponent>().Position  = { -300.f, 0.f };
    lRight.Get<TransformComponent>().Position = { 300.f, 100.f };

    Entity            lCamera = lRig.MakeCamera("Group");
    CameraGroupStage& lGroup  = lCamera.Add<CameraGroupStage>();
    lGroup.Targets            = { OpaaxString("Left"), OpaaxString("Nobody"), OpaaxString("Right") };
    lGroup.Padding            = 50.f;
    lGroup.MinSize            = 100.f;

    // 350 each side at aspect 2: 175 tall.
    lRig.Tick(1);
    CheckNear(lCamera.Get<TransformComponent>().Position, { 0.f, 50.f });
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(175.f));
    REQUIRE(lGroup.GetTargetBounds().has_value());
    CheckNear(lGroup.GetTargetBounds()->HalfExtent, { 300.f, 50.f });

    // Right runs off: 650 each side, 325 tall.
    lRight.Get<TransformComponent>().Position = { 900.f, 100.f };
    lRig.Tick(1);
    CheckNear(lCamera.Get<TransformComponent>().Position, { 300.f, 50.f });
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(325.f));

    // Left gone: Right alone, zoomed in to MinSize.
    lLeft.Destroy();
    lRig.Tick(1);
    CheckNear(lCamera.Get<TransformComponent>().Position, { 900.f, 100.f });
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(100.f));

    // Eased: part of the way after a step, toward the group.
    lGroup.Damping = 0.5f;
    lRight.Get<TransformComponent>().Position = { 0.f, 100.f };
    lRig.Tick(1);
    const float lX = lCamera.Get<TransformComponent>().Position.x;
    CHECK(lX < 900.f);
    CHECK(lX > 0.f);
}

// =============================================================================
// Zones
// =============================================================================
namespace
{
    CameraZone MakeZone(const Vector2F InCenter, const Vector2F InHalfExtent, const float InOrthoSize,
                        const Int32 InPriority, const float InBlendTime, const Uint32 InId)
    {
        return CameraZone{ static_cast<EntityID>(InId), Bounds2D{ InCenter, InHalfExtent }, InOrthoSize,
                           /*bConfine*/ true, InPriority, InBlendTime };
    }
}

TEST_CASE("PickCameraZone: the highest priority holding the point, the first among equals; none outside")
{
    const TDynArray<CameraZone> lZones{ MakeZone({ 0.f, 0.f }, { 500.f, 300.f }, 0.f, 0, 0.5f, 1),
                                        MakeZone({ 400.f, 0.f }, { 200.f, 300.f }, 0.f, 2, 0.5f, 2),
                                        MakeZone({ 400.f, 0.f }, { 200.f, 300.f }, 2.f, 2, 0.5f, 3) };

    REQUIRE(PickCameraZone(lZones, { -100.f, 0.f }) != nullptr);
    CHECK(PickCameraZone(lZones, { -100.f, 0.f })->Entity == static_cast<EntityID>(1));
    CHECK(PickCameraZone(lZones, { 450.f, 0.f })->Entity == static_cast<EntityID>(2));
    CHECK(PickCameraZone(lZones, { 5000.f, 0.f }) == nullptr);
}

TEST_CASE("StepCameraZone: a zone eases in and out over its BlendTime, ending exact; a zone changed mid-blend never jumps")
{
    constexpr float WIDE = 16.f / 9.f;

    // Given at 5000 every step, as a body stage locked on a target far right would give it.
    const CameraRigPose GIVEN{ { 5000.f, 0.f }, 0.f, 300.f };
    const CameraZone    ROOM = MakeZone({ 0.f, 0.f }, { 1000.f, 600.f }, 500.f, 0, 0.5f, 1);

    CameraZoneState lState;
    CameraRigPose   lLast = GIVEN;
    float           lMostMove = 0.f;
    float           lMostZoom = 0.f;
    for (Int32 lStep = 0; lStep < 40; ++lStep)
    {
        CameraRigPose lPose = GIVEN;
        StepCameraZone(lPose, &ROOM, lState, WIDE, DT);
        lMostMove = Maths::Max(lMostMove, Maths::Abs(lPose.Position.x - lLast.Position.x));
        lMostZoom = Maths::Max(lMostZoom, Maths::Abs(lPose.OrthoSize - lLast.OrthoSize));
        CHECK(lPose.OrthoSize >= lLast.OrthoSize);
        lLast = lPose;
    }

    // In 0.5 s: sized and confined exactly, 1000 - 500 * 16 / 9 at most. Eased in and out, no step
    // moves more than 1.5 times an even share (30 steps).
    CHECK(lLast.OrthoSize == doctest::Approx(500.f));
    CHECK(lLast.Position.x == doctest::Approx(1000.f - 500.f * WIDE));
    CHECK(lMostZoom < 1.5f * 200.f / 30.f + 0.01f);
    CHECK(lMostMove < 1.5f * (5000.f - lLast.Position.x) / 30.f + 0.01f);

    // Left for none: back over the room's BlendTime to the size and place given.
    for (Int32 lStep = 0; lStep < 40; ++lStep)
    {
        lLast = GIVEN;
        StepCameraZone(lLast, nullptr, lState, WIDE, DT);
    }
    CHECK(lLast.OrthoSize == doctest::Approx(300.f));
    CHECK(lLast.Position.x == doctest::Approx(5000.f));

    // Into the room, then into a smaller one a third of the way: the hand-over step is where the room
    // alone puts the view, and the closet's blend starts from there, easing in.
    const CameraZone CLOSET = MakeZone({ 4000.f, 0.f }, { 200.f, 600.f }, 150.f, 1, 0.5f, 2);
    TDynArray<float> lXs;
    CameraZoneState  lRoomOnly;
    for (Int32 lStep = 0; lStep < 60; ++lStep)
    {
        if (lStep == 10)
        {
            lRoomOnly = lState;
        }
        CameraRigPose lPose = GIVEN;
        StepCameraZone(lPose, lStep < 10 ? &ROOM : &CLOSET, lState, WIDE, DT);
        lXs.push_back(lPose.Position.x);
    }
    CameraRigPose lRoomStep = GIVEN;
    StepCameraZone(lRoomStep, &ROOM, lRoomOnly, WIDE, DT);
    CHECK(lXs[10] == doctest::Approx(lRoomStep.Position.x));
    CHECK(Maths::Abs(lXs[11] - lXs[10]) < Maths::Abs(lXs[10] - lXs[9]));
    CHECK(lXs.back() == doctest::Approx(4000.f));   // the closet is narrower than the view: centred on it
}

TEST_CASE("CameraZoneStage: the zone holding its target sizes and bounds the view; out of every zone, it keeps the size")
{
    RigWorld lRig;
    Entity   lRunner = lRig.TheWorld->CreateEntity("Runner");

    // Two rooms side by side, cut to (BlendTime 0): 0 to 1000 at size 100, 1000 to 2000 at size 200.
    const auto lRoom = [&lRig](const char* InName, const float InX, const float InSize)
    {
        Entity lEntity = lRig.TheWorld->CreateEntity(InName);
        lEntity.Get<TransformComponent>().Position = { InX, 0.f };
        CameraZoneComponent& lZone = lEntity.Add<CameraZoneComponent>();
        lZone.HalfExtent           = { 500.f, 300.f };
        lZone.OrthoSize            = InSize;
        lZone.BlendTime            = 0.f;
        return lEntity;
    };
    lRoom("Small", 500.f, 100.f);
    lRoom("Large", 1500.f, 200.f);

    Entity lCamera = lRig.MakeCamera("Roomed");
    lCamera.Add<CameraFollowStage>().Target = "Runner";
    lCamera.Add<CameraZoneStage>().Target   = "Runner";

    const auto lAt = [&](const float InX)
    {
        lRunner.Get<TransformComponent>().Position = { InX, 0.f };
        lRig.Tick(1);
        return lCamera.Get<TransformComponent>().Position.x;
    };

    // 16:9 before any frame: half a size-100 view is 177.8 wide.
    CHECK(lAt(300.f) == doctest::Approx(300.f));
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(100.f));
    CHECK(lAt(900.f) == doctest::Approx(1000.f - 100.f * 16.f / 9.f));
    CHECK(lAt(1100.f) == doctest::Approx(1000.f + 200.f * 16.f / 9.f));
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(200.f));
    CHECK(lAt(2500.f) == doctest::Approx(2500.f));
    CHECK(lCamera.Get<CameraComponent>().OrthoSize == doctest::Approx(200.f));
}
