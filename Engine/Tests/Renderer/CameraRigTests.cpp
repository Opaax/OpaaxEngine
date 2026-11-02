// Suite: CameraRigSubsystem — a camera's stages run in each fixed step by stage (Body, Size, then
// Constraint) whatever order they were added in, each on the pose the ones before it left, and the
// result is written to the camera once, in the world: a rigged camera moves in the steps only, so
// its rhythm is "steps" at any frames. Driven through WorldManager the way Engine::Loop runs a frame.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/Camera/CameraComponent.h"
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
