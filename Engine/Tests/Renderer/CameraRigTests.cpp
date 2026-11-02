// Suite: CameraRigSubsystem — a camera's stages run in each fixed step by stage (Body, Size, then
// Constraint) whatever order they were added in, each on the pose the ones before it left, and the
// result is written to the camera once, in the world: a rigged camera moves in the steps only, so
// its rhythm is "steps" at any frames. Driven through WorldManager the way Engine::Loop runs a frame.
// CameraFollowStage: a dead zone, damping per axis and a look-ahead, the same over one step or two halves.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraFollowStage.h"
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
