// Suite: a camera following a moving body keeps it at the same place on screen, and a camera zoomed
// in fixed steps is framed with its size between its last two steps, for any alpha and any number
// of fixed steps in a frame. Driven through WorldManager the way Engine::Loop runs a frame:
// Update, CameraManager's Update, the steps, then the view resolved at the frame's alpha.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolation.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Components/TransformComponent.h"
#include "World/Components/TransformInterpolationComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/TransformInterpolation.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    constexpr double STEP_SECONDS = 1.0 / 60.0;

    /** Full momentum: 30 units a step. */
    constexpr float SPEED = 1800.f;

    /** Where the camera sits from the body. */
    const Vector2F CAMERA_OFFSET{ 0.f, 40.f };

    /** OrthoSize a zooming camera gains every step. */
    constexpr float ZOOM_PER_STEP = 6.f;

    // Moves the body at a constant speed every fixed step, recording its previous pose first, as
    // the mover does.
    class RunnerSubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(RunnerSubsystem, WorldSubsystem)

    public:
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::PostPhysics; }

        void FixedUpdate(const double InFixedDeltaTime) override
        {
            Entity lBody{ Body, &GetWorld() };
            if (!lBody.IsValid()) { return; }

            TransformComponent&              lLocal    = lBody.Get<TransformComponent>();
            TransformInterpolationComponent& lPrevious = lBody.Get<TransformInterpolationComponent>();

            lPrevious.Position     = lLocal.Position;
            lPrevious.Rotation     = lLocal.Rotation;
            lPrevious.bHasPrevious = true;

            lLocal.Position.x += SPEED * static_cast<float>(InFixedDeltaTime);
        }

        EntityID Body = ENTITY_NONE;
    };

    // Puts the camera on the body every fixed step, after it moved, as a game's camera subsystem does.
    class ChaseCameraSubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(ChaseCameraSubsystem, WorldSubsystem)

    public:
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Presentation; }

        void FixedUpdate(double) override
        {
            Entity lBody{ Body, &GetWorld() };
            Entity lCamera{ Camera, &GetWorld() };
            if (!lBody.IsValid() || !lCamera.IsValid()) { return; }

            lCamera.Get<TransformComponent>().Position = lBody.Get<TransformComponent>().Position + CAMERA_OFFSET;
        }

        EntityID Body   = ENTITY_NONE;
        EntityID Camera = ENTITY_NONE;
    };

    // Grows the camera's OrthoSize every fixed step, as a game's zoom eased in its steps does.
    class ZoomCameraSubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(ZoomCameraSubsystem, WorldSubsystem)

    public:
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Presentation; }

        void FixedUpdate(double) override
        {
            Entity lCamera{ Camera, &GetWorld() };
            if (!lCamera.IsValid()) { return; }

            lCamera.Get<CameraComponent>().OrthoSize += PerStep;
        }

        EntityID Camera  = ENTITY_NONE;
        float    PerStep = 0.f;   // no zoom unless asked
    };

    enum class EFollow : Uint8
    {
        Parented,   // the camera is a child of the body
        Chased      // a subsystem moves the camera in every fixed step
    };

    // A bare manager builds worlds with no subsystems, so they are added the way
    // CreateSubsystemsFor would: context first, then the classes, then StartupAll.
    struct ChaseWorld
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;

        World*   TheWorld = nullptr;
        Entity   Body;
        Entity   Camera;
        EntityID Recorded = ENTITY_NONE;

        explicit ChaseWorld(const EFollow InFollow)
        {
            TheWorld = Worlds.CreateWorld("Chase", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events, Input, Config,
                                               /*Actions*/ nullptr, /*UI*/ nullptr, Debug });
            TheWorld->GetSubsystems().Add<RunnerSubsystem>();
            if (InFollow == EFollow::Chased)
            {
                TheWorld->GetSubsystems().Add<ChaseCameraSubsystem>();
            }
            TheWorld->GetSubsystems().Add<ZoomCameraSubsystem>();
            TheWorld->GetSubsystems().StartupAll();

            Body = TheWorld->CreateEntity("Body");
            Body.Add<TransformInterpolationComponent>();

            Camera = TheWorld->CreateEntity("Camera");
            Camera.Add<CameraComponent>();
            Camera.Get<TransformComponent>().Position = CAMERA_OFFSET;

            if (InFollow == EFollow::Parented)
            {
                REQUIRE(EntityHierarchy::SetParent(Camera, Body, /*bKeepWorld*/ true));
            }

            RunnerSubsystem* lRunner = TheWorld->GetSubsystems().GetSubsystem<RunnerSubsystem>();
            REQUIRE(lRunner != nullptr);
            lRunner->Body = Body.GetHandle();

            if (ChaseCameraSubsystem* lChaser = TheWorld->GetSubsystems().GetSubsystem<ChaseCameraSubsystem>())
            {
                lChaser->Body   = Body.GetHandle();
                lChaser->Camera = Camera.GetHandle();
            }

            ZoomCameraSubsystem* lZoom = TheWorld->GetSubsystems().GetSubsystem<ZoomCameraSubsystem>();
            REQUIRE(lZoom != nullptr);
            lZoom->Camera = Camera.GetHandle();

            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        /** Grows the camera's OrthoSize by InPerStep every fixed step. */
        void ZoomEachStep(const float InPerStep)
        {
            TheWorld->GetSubsystems().GetSubsystem<ZoomCameraSubsystem>()->PerStep = InPerStep;
        }

        float CameraSize() { return Camera.Get<CameraComponent>().OrthoSize; }

        /**
         * Update, then the camera manager's update (it runs after the worlds' and records the camera
         * framing the world), then InSteps steps.
         */
        void Tick(const Uint32 InSteps)
        {
            Worlds.Update(STEP_SECONDS);

            const EntityID lActive = CameraManager::Resolve(*TheWorld).Entity;
            CameraManager::RecordCameraSteps(*TheWorld, lActive, Recorded);
            Recorded = lActive;

            for (Uint32 lStep = 0; lStep < InSteps; ++lStep)
            {
                Worlds.FixedUpdate(STEP_SECONDS);
            }
        }

        /** Where the body is drawn from the centre of the view, at InAlpha. */
        Vector2F OnScreen(const float InAlpha)
        {
            const Vector2F lView  = CameraManager::Resolve(*TheWorld, InAlpha).View.Position;
            const Vector2F lDrawn = EntityHierarchy::DisplayTransform(Body, InAlpha).Position;
            return lDrawn - lView;
        }
    };

    /** Frames as a clock gives them: zero to three steps, and the alpha left over. */
    struct Frame
    {
        Uint32 Steps = 1;
        float  Alpha = 0.f;
    };

    constexpr Frame FRAMES[] = {
        { 1, 0.10f }, { 0, 0.45f }, { 0, 0.80f }, { 1, 0.15f }, { 2, 0.60f },
        { 3, 0.05f }, { 0, 0.99f }, { 1, 0.50f }, { 2, 0.00f }, { 1, 1.00f },
    };
}

TEST_CASE("Camera interpolation: a camera following a moving body keeps it at the same place on screen")
{
    for (const EFollow lFollow : { EFollow::Parented, EFollow::Chased })
    {
        CAPTURE(static_cast<int>(lFollow));
        ChaseWorld lChase(lFollow);

        // A first step, so every previous pose exists.
        lChase.Tick(1);

        for (const Frame& lFrame : FRAMES)
        {
            CAPTURE(lFrame.Steps);
            CAPTURE(lFrame.Alpha);

            lChase.Tick(lFrame.Steps);

            const Vector2F lOnScreen = lChase.OnScreen(lFrame.Alpha);
            CHECK(lOnScreen.x == doctest::Approx(-CAMERA_OFFSET.x).epsilon(1e-4));
            CHECK(lOnScreen.y == doctest::Approx(-CAMERA_OFFSET.y).epsilon(1e-4));
        }
    }
}

TEST_CASE("Camera interpolation: framed from the current pose, the body would slide on screen")
{
    // Without the alpha the camera sits on the body's current pose while the body is drawn between
    // its last two: the offset the test above rules out.
    ChaseWorld lChase(EFollow::Parented);
    lChase.Tick(1);
    lChase.Tick(2);

    const Vector2F lView  = CameraManager::Resolve(*lChase.TheWorld).View.Position;
    const Vector2F lDrawn = EntityHierarchy::DisplayTransform(lChase.Body, 0.5f).Position;

    // Half a step behind the camera: 15 units off.
    CHECK((lDrawn - lView).x == doctest::Approx(-15.f));
}

TEST_CASE("Camera interpolation: a camera moved in Update is framed where Update put it")
{
    ChaseWorld lChase(EFollow::Chased);
    lChase.TheWorld->GetSubsystems().GetSubsystem<ChaseCameraSubsystem>()->Camera = ENTITY_NONE;   // no longer chased
    lChase.Tick(1);
    lChase.Tick(1);

    // A frame with no step: the move made in Update is shown as it is, at any alpha.
    lChase.Tick(0);
    lChase.Camera.Get<TransformComponent>().Position.x += 25.f;

    const float lMovedTo = lChase.Camera.Get<TransformComponent>().Position.x;

    for (const float lAlpha : { 0.f, 0.3f, 0.9f })
    {
        CHECK(CameraManager::Resolve(*lChase.TheWorld, lAlpha).View.Position.x == doctest::Approx(lMovedTo));
    }

    // A frame with a step: still where it was put.
    lChase.Camera.Get<TransformComponent>().Position.x += 25.f;
    lChase.Tick(1);
    CHECK(CameraManager::Resolve(*lChase.TheWorld, 0.4f).View.Position.x == doctest::Approx(lMovedTo + 25.f));
}

TEST_CASE("Camera interpolation: a camera zoomed in fixed steps is framed with its size between its last two steps")
{
    for (const EFollow lFollow : { EFollow::Parented, EFollow::Chased })
    {
        CAPTURE(static_cast<int>(lFollow));
        ChaseWorld lChase(lFollow);
        lChase.ZoomEachStep(ZOOM_PER_STEP);

        const float lStart = lChase.CameraSize();

        lChase.Tick(1);
        Uint32 lSteps = 1;

        for (const Frame& lFrame : FRAMES)
        {
            CAPTURE(lFrame.Steps);
            CAPTURE(lFrame.Alpha);

            lChase.Tick(lFrame.Steps);
            lSteps += lFrame.Steps;

            // Between the size before the last step and the size it left.
            const float lBlended = lStart + (static_cast<float>(lSteps) - 1.f + lFrame.Alpha) * ZOOM_PER_STEP;
            CHECK(CameraManager::Resolve(*lChase.TheWorld, lFrame.Alpha).View.OrthoSize == doctest::Approx(lBlended));

            // Interpolation off: the current size.
            CHECK(CameraManager::Resolve(*lChase.TheWorld).View.OrthoSize
                  == doctest::Approx(lStart + static_cast<float>(lSteps) * ZOOM_PER_STEP));
        }
    }
}

TEST_CASE("Camera interpolation: a size set in Update is framed as it is")
{
    ChaseWorld lChase(EFollow::Parented);
    lChase.ZoomEachStep(ZOOM_PER_STEP);
    lChase.Tick(1);
    lChase.Tick(1);

    // A frame with no step: the size set in Update is framed as it is, at any alpha.
    lChase.Tick(0);
    lChase.Camera.Get<CameraComponent>().OrthoSize += 25.f;

    const float lSetTo = lChase.CameraSize();

    for (const float lAlpha : { 0.f, 0.3f, 0.9f })
    {
        CHECK(CameraManager::Resolve(*lChase.TheWorld, lAlpha).View.OrthoSize == doctest::Approx(lSetTo));
    }

    // The next step zooms from there.
    lChase.Tick(1);

    for (const float lAlpha : { 0.f, 0.3f, 0.9f })
    {
        CHECK(CameraManager::Resolve(*lChase.TheWorld, lAlpha).View.OrthoSize
              == doctest::Approx(lSetTo + lAlpha * ZOOM_PER_STEP));
    }
}

TEST_CASE("Camera interpolation: only the camera framing the world is recorded, however many there are")
{
    World lWorld("Crowd");

    Entity lActive = lWorld.CreateEntity("Active");
    lActive.Add<CameraComponent>().Priority = 1;

    TDynArray<Entity> lIdle;
    for (int lIndex = 0; lIndex < 100; ++lIndex)
    {
        Entity lCamera = lWorld.CreateEntity("Idle");
        lCamera.Add<CameraComponent>();
        lIdle.push_back(lCamera);
    }

    EntityID lRecorded = CameraManager::Resolve(lWorld).Entity;
    REQUIRE(lRecorded == lActive.GetHandle());
    CameraManager::RecordCameraSteps(lWorld, lRecorded, ENTITY_NONE);

    CHECK(lActive.Get<TransformInterpolationComponent>().bRecordSteps);
    CHECK(lActive.Has<CameraInterpolationComponent>());
    for (Entity& lCamera : lIdle)
    {
        CHECK(lCamera.TryGet<TransformInterpolationComponent>() == nullptr);
        CHECK_FALSE(lCamera.Has<CameraInterpolationComponent>());
    }

    // A step records the active camera.
    lActive.Get<TransformComponent>().Position.x = 10.f;
    TransformInterpolation::BeginFixedStep(lWorld);
    CameraInterpolation::BeginFixedStep(lWorld);
    lActive.Get<CameraComponent>().OrthoSize = 320.f;
    TransformInterpolation::EndFixedStep(lWorld);
    CameraInterpolation::EndFixedStep(lWorld);
    REQUIRE(lActive.Get<TransformInterpolationComponent>().bHasPrevious);

    const CameraInterpolationComponent& lSizes = lActive.Get<CameraInterpolationComponent>();
    CHECK(lSizes.bHasStep);
    CHECK(lSizes.PreviousOrthoSize == doctest::Approx(300.f));
    CHECK(lSizes.SteppedOrthoSize == doctest::Approx(320.f));

    // Another wins the view: it is recorded, and the old one forgets its poses.
    lIdle[7].Get<CameraComponent>().Priority = 5;
    const EntityID lNext = CameraManager::Resolve(lWorld).Entity;
    REQUIRE(lNext == lIdle[7].GetHandle());
    CameraManager::RecordCameraSteps(lWorld, lNext, lRecorded);

    CHECK(lIdle[7].Get<TransformInterpolationComponent>().bRecordSteps);
    CHECK(lIdle[7].Has<CameraInterpolationComponent>());

    const TransformInterpolationComponent& lOld = lActive.Get<TransformInterpolationComponent>();
    CHECK_FALSE(lOld.bRecordSteps);
    CHECK_FALSE(lOld.bHasPrevious);
    CHECK_FALSE(lOld.bHasStepped);
    CHECK_FALSE(lActive.Has<CameraInterpolationComponent>());
}

TEST_CASE("ResolveDisplayOrthoSize: no step recorded answers the current size")
{
    CHECK(ResolveDisplayOrthoSize(400.f, nullptr, 0.5f) == 400.f);

    // Recorded but no step yet: blending from a default size would zoom in from 0.
    const CameraInterpolationComponent lUnwritten{};
    CHECK(ResolveDisplayOrthoSize(400.f, &lUnwritten, 0.5f) == 400.f);
}

TEST_CASE("ResolveDisplayOrthoSize: alpha 0 is the size before the step, 1 the size it left, clamped")
{
    CameraInterpolationComponent lHistory;
    lHistory.PreviousOrthoSize = 300.f;
    lHistory.SteppedOrthoSize  = 340.f;
    lHistory.bHasStep          = true;

    CHECK(ResolveDisplayOrthoSize(340.f, &lHistory, 0.f) == doctest::Approx(300.f));
    CHECK(ResolveDisplayOrthoSize(340.f, &lHistory, 0.25f) == doctest::Approx(310.f));
    CHECK(ResolveDisplayOrthoSize(340.f, &lHistory, 1.f) == doctest::Approx(340.f));

    CHECK(ResolveDisplayOrthoSize(340.f, &lHistory, 2.f) == doctest::Approx(340.f));
    CHECK(ResolveDisplayOrthoSize(340.f, &lHistory, -1.f) == doctest::Approx(300.f));

    // Set since the step (in Update): the size as it is, at any alpha.
    CHECK(ResolveDisplayOrthoSize(500.f, &lHistory, 0.f) == 500.f);
    CHECK(ResolveDisplayOrthoSize(500.f, &lHistory, 0.6f) == 500.f);
}
