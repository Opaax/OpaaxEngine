// Suite: a camera following a moving body keeps it at the same place on screen, also on the frame
// another camera takes the view over, and a camera zoomed in fixed steps is framed with its size
// between its last two steps, for any alpha and any number of fixed steps in a frame. Driven through
// WorldManager the way Engine::Loop runs a frame: Update, CameraManager's Update, the steps, then the
// view resolved at the frame's alpha.
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
#include "Renderer/Camera/CameraRhythm.h"
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

    /** Where the camera sits from the body, and a second camera chasing it. */
    const Vector2F CAMERA_OFFSET{ 0.f, 40.f };
    const Vector2F OTHER_OFFSET{ 0.f, -40.f };

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

            Entity lOther{ Other, &GetWorld() };
            if (lOther.IsValid())
            {
                lOther.Get<TransformComponent>().Position = lBody.Get<TransformComponent>().Position + OTHER_OFFSET;
            }
        }

        EntityID Body   = ENTITY_NONE;
        EntityID Camera = ENTITY_NONE;
        EntityID Other  = ENTITY_NONE;   // a second chased camera, when asked
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

        /** A second camera, chased at OTHER_OFFSET in every step, below the first in priority. */
        Entity AddOtherCamera()
        {
            Camera.Get<CameraComponent>().Priority = 1;

            Entity lOther = TheWorld->CreateEntity("Other");
            lOther.Add<CameraComponent>();
            lOther.Get<TransformComponent>().Position = OTHER_OFFSET;
            TheWorld->GetSubsystems().GetSubsystem<ChaseCameraSubsystem>()->Other = lOther.GetHandle();
            return lOther;
        }

        /**
         * Update, then the camera manager's update (it runs after the worlds' and records the
         * cameras), then InSteps steps.
         */
        void Tick(const Uint32 InSteps)
        {
            Worlds.Update(STEP_SECONDS);
            CameraManager::RecordCameraSteps(*TheWorld);

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

TEST_CASE("Camera interpolation: a camera taking the view over keeps the body still from its first frame")
{
    // The switch lands on every frame of FRAMES in turn: a frame of steps or none, any alpha.
    for (Uint64 lSwitchAt = 0; lSwitchAt < sizeof(FRAMES) / sizeof(FRAMES[0]); ++lSwitchAt)
    {
        CAPTURE(lSwitchAt);
        ChaseWorld lChase(EFollow::Chased);
        Entity     lOther = lChase.AddOtherCamera();
        lChase.Tick(1);

        for (Uint64 lIndex = 0; lIndex < sizeof(FRAMES) / sizeof(FRAMES[0]); ++lIndex)
        {
            const Frame& lFrame = FRAMES[lIndex];
            CAPTURE(lIndex);

            // Gameplay raises the other camera in its update, as a cutscene trigger would.
            if (lIndex == lSwitchAt)
            {
                lOther.Get<CameraComponent>().Priority = 2;
            }
            lChase.Tick(lFrame.Steps);

            const Vector2F lOffset   = lIndex >= lSwitchAt ? OTHER_OFFSET : CAMERA_OFFSET;
            const Vector2F lOnScreen = lChase.OnScreen(lFrame.Alpha);
            CHECK(lOnScreen.x == doctest::Approx(-lOffset.x).epsilon(1e-4));
            CHECK(lOnScreen.y == doctest::Approx(-lOffset.y).epsilon(1e-4));
        }
    }
}

TEST_CASE("ResolveCamera: the camera framing the world gives Resolve's view; another gives its own")
{
    ChaseWorld lChase(EFollow::Chased);
    Entity     lOther = lChase.AddOtherCamera();
    lChase.ZoomEachStep(ZOOM_PER_STEP);
    lChase.Tick(1);

    for (const Frame& lFrame : FRAMES)
    {
        CAPTURE(lFrame.Steps);
        CAPTURE(lFrame.Alpha);
        lChase.Tick(lFrame.Steps);

        for (const TOptional<float> lAlpha : { TOptional<float>(lFrame.Alpha), TOptional<float>() })
        {
            const CameraResolution lWorld = CameraManager::Resolve(*lChase.TheWorld, lAlpha);
            const CameraView       lMain  = CameraManager::ResolveCamera(lChase.Camera, lAlpha);
            REQUIRE(lWorld.Entity == lChase.Camera.GetHandle());
            CHECK(lMain.Position.x == lWorld.View.Position.x);
            CHECK(lMain.Position.y == lWorld.View.Position.y);
            CHECK(lMain.OrthoSize == lWorld.View.OrthoSize);
        }

        // The other camera, not framing the world, keeps the body still in its own view too.
        const Vector2F lOtherView = CameraManager::ResolveCamera(lOther, lFrame.Alpha).Position;
        const Vector2F lDrawn     = EntityHierarchy::DisplayTransform(lChase.Body, lFrame.Alpha).Position;
        CHECK(lDrawn.y - lOtherView.y == doctest::Approx(-OTHER_OFFSET.y));
        CHECK(lDrawn.x - lOtherView.x == doctest::Approx(0.f).epsilon(1e-4));
    }

    CHECK(CameraManager::ResolveCamera(lChase.Body).OrthoSize == CameraView{}.OrthoSize);
}

TEST_CASE("Cut: cameras placed in a step and cut are framed where they are at every alpha, then move on")
{
    World lWorld("Respawn");

    Entity lBody   = lWorld.CreateEntity("Body");
    Entity lCamera = lWorld.CreateEntity("Camera");
    lCamera.Add<CameraComponent>();
    TransformInterpolation::RecordSteps(lBody);
    CameraManager::RecordCameraSteps(lWorld);

    const auto lStep = [&](const float InX, const float InSize, const bool bInRespawn)
    {
        TransformInterpolation::BeginFixedStep(lWorld);
        CameraInterpolation::BeginFixedStep(lWorld);
        lBody.Get<TransformComponent>().Position.x   = InX;
        lCamera.Get<TransformComponent>().Position.x = InX;
        lCamera.Get<CameraComponent>().OrthoSize     = InSize;
        if (bInRespawn)
        {
            TransformInterpolation::Teleport(lBody);
            CameraManager::Cut(lWorld);
        }
        TransformInterpolation::EndFixedStep(lWorld);
        CameraInterpolation::EndFixedStep(lWorld);
    };

    lStep(30.f, 310.f, false);
    CHECK(CameraManager::Resolve(lWorld, 0.5f).View.Position.x == doctest::Approx(15.f));
    CHECK_FALSE(lCamera.Get<CameraInterpolationComponent>().bCut);

    // The respawn: framed at the new place and size, the body still in the middle, the frame marked.
    lStep(-900.f, 300.f, true);
    for (const float lAlpha : { 0.f, 0.5f, 1.f })
    {
        CAPTURE(lAlpha);
        const CameraView lView = CameraManager::Resolve(lWorld, lAlpha).View;
        CHECK(lView.Position.x == doctest::Approx(-900.f));
        CHECK(lView.OrthoSize == doctest::Approx(300.f));
        CHECK(EntityHierarchy::DisplayTransform(lBody, lAlpha).Position.x - lView.Position.x == doctest::Approx(0.f));
    }
    CHECK(lCamera.Get<CameraInterpolationComponent>().bCut);

    // On from there.
    lStep(-870.f, 320.f, false);
    CHECK(CameraManager::Resolve(lWorld, 0.5f).View.Position.x == doctest::Approx(-885.f));
    CHECK(CameraManager::Resolve(lWorld, 0.5f).View.OrthoSize == doctest::Approx(310.f));
}

TEST_CASE("Camera interpolation: every camera is recorded, however many there are")
{
    World lWorld("Crowd");

    TDynArray<Entity> lCameras;
    for (int lIndex = 0; lIndex < 100; ++lIndex)
    {
        Entity lCamera = lWorld.CreateEntity("Camera");
        lCamera.Add<CameraComponent>().Priority = lIndex == 42 ? 1 : 0;
        lCameras.push_back(lCamera);
    }

    CameraManager::RecordCameraSteps(lWorld);
    CameraManager::RecordCameraSteps(lWorld);   // again: nothing more

    for (Entity& lCamera : lCameras)
    {
        CHECK(lCamera.Get<TransformInterpolationComponent>().bRecordSteps);
        CHECK(lCamera.Has<CameraInterpolationComponent>());
    }

    // A step records them all, framing the world or not.
    lCameras[7].Get<TransformComponent>().Position.x = 10.f;
    TransformInterpolation::BeginFixedStep(lWorld);
    CameraInterpolation::BeginFixedStep(lWorld);
    lCameras[7].Get<CameraComponent>().OrthoSize = 320.f;
    TransformInterpolation::EndFixedStep(lWorld);
    CameraInterpolation::EndFixedStep(lWorld);

    CHECK(lCameras[7].Get<TransformInterpolationComponent>().bHasPrevious);
    const CameraInterpolationComponent& lSizes = lCameras[7].Get<CameraInterpolationComponent>();
    CHECK(lSizes.bHasStep);
    CHECK(lSizes.PreviousOrthoSize == doctest::Approx(300.f));
    CHECK(lSizes.SteppedOrthoSize == doctest::Approx(320.f));
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

// =============================================================================
// Rhythm: a camera moved both in the steps and between them is found, at any frame rate
// =============================================================================
namespace
{
    /** The steps each frame takes, repeating: 60, about 144 and 30 frames a second at 60 steps. */
    struct StepPattern
    {
        const char*       Name;
        TDynArray<Uint32> Steps;
    };

    const StepPattern PATTERNS[] = { { "60", { 1 } }, { "144", { 1, 0, 1, 0, 0 } }, { "30", { 2 } } };

    constexpr Uint32 THREE_WINDOWS = 3 * CameraRhythm::WINDOW_STEPS;

    /** Plays frames of InPattern until InSteps steps ran, InUpdate(frame) run before each as gameplay's Update. */
    template <typename TUpdate>
    void PlaySteps(ChaseWorld& InChase, const StepPattern& InPattern, const Uint32 InSteps, TUpdate&& InUpdate)
    {
        Uint32 lSteps = 0;
        for (Uint32 lFrame = 0; lSteps < InSteps; ++lFrame)
        {
            InUpdate(lFrame);
            const Uint32 lFrameSteps = InPattern.Steps[lFrame % InPattern.Steps.size()];
            InChase.Tick(lFrameSteps);
            lSteps += lFrameSteps;
        }
    }

    const CameraRhythm& RhythmOf(Entity InCamera) { return InCamera.Get<CameraInterpolationComponent>().Rhythm; }
}

TEST_CASE("ObserveCameraRhythm: each window of steps gives the rhythm; a mixed one warns once")
{
    const EntityID lMover = static_cast<EntityID>(3);

    CameraRhythm lRhythm;
    const auto lWindow = [&lRhythm](const CameraStepMotion& InMotion, const Uint32 InTimes, const CameraStepMotion& InRest)
    {
        bool bWarn = false;
        for (Uint32 lStep = 0; lStep < CameraRhythm::WINDOW_STEPS; ++lStep)
        {
            bWarn = ObserveCameraRhythm(lRhythm, lStep < InTimes ? InMotion : InRest) || bWarn;
        }
        return bWarn;
    };

    const CameraStepMotion lStill;
    const CameraStepMotion lInStep{ true, false };
    const CameraStepMotion lSince{ false, true };
    const CameraStepMotion lBoth{ true, true, lMover, false };

    // Unknown before a full window: not "still".
    for (Uint32 lStep = 1; lStep < CameraRhythm::WINDOW_STEPS; ++lStep)
    {
        CHECK_FALSE(ObserveCameraRhythm(lRhythm, lInStep));
    }
    CHECK(lRhythm.Last == ECameraRhythm::Unknown);
    CHECK(OpaaxStringView(ToString(lRhythm.Last)) == OpaaxStringView("unknown"));
    CHECK_FALSE(ObserveCameraRhythm(lRhythm, lInStep));
    CHECK(lRhythm.Last == ECameraRhythm::Steps);

    CHECK_FALSE(lWindow(lSince, CameraRhythm::WINDOW_STEPS, lSince));
    CHECK(lRhythm.Last == ECameraRhythm::Update);

    CHECK_FALSE(lWindow(lStill, CameraRhythm::WINDOW_STEPS, lStill));
    CHECK(lRhythm.Last == ECameraRhythm::Still);

    // A few steps moved both ways (a respawn, a hand-over) are not a habit.
    CHECK_FALSE(lWindow(lBoth, CameraRhythm::MIXED_STEPS - 1, lInStep));
    CHECK(lRhythm.Last == ECameraRhythm::Steps);

    // Enough are: warned once, named.
    CHECK(lWindow(lBoth, CameraRhythm::MIXED_STEPS, lInStep));
    CHECK(lRhythm.Last == ECameraRhythm::Mixed);
    CHECK(lRhythm.MovedBoth == lMover);
    CHECK_FALSE(lRhythm.bSizeBoth);
    CHECK_FALSE(lWindow(lBoth, CameraRhythm::WINDOW_STEPS, lBoth));
    CHECK(lRhythm.Last == ECameraRhythm::Mixed);
    CHECK(lRhythm.bWarned);

    CHECK(OpaaxStringView(ToString(ECameraRhythm::Mixed)) == OpaaxStringView("mixed"));
}

TEST_CASE("Camera rhythm: a camera chased in the steps is 'steps', one moved in Update is 'update'; neither warns")
{
    for (const StepPattern& lPattern : PATTERNS)
    {
        CAPTURE(lPattern.Name);

        for (const EFollow lFollow : { EFollow::Parented, EFollow::Chased })
        {
            CAPTURE(static_cast<int>(lFollow));
            ChaseWorld lChase(lFollow);
            PlaySteps(lChase, lPattern, THREE_WINDOWS, [](Uint32) {});

            CHECK(RhythmOf(lChase.Camera).Last == ECameraRhythm::Steps);
            CHECK_FALSE(RhythmOf(lChase.Camera).bWarned);
        }

        // Not chased: put somewhere else in every Update.
        ChaseWorld lChase(EFollow::Chased);
        lChase.TheWorld->GetSubsystems().GetSubsystem<ChaseCameraSubsystem>()->Camera = ENTITY_NONE;
        PlaySteps(lChase, lPattern, THREE_WINDOWS, [&lChase](Uint32) { lChase.Camera.Get<TransformComponent>().Position.x += 4.f; });

        CHECK(RhythmOf(lChase.Camera).Last == ECameraRhythm::Update);
        CHECK_FALSE(RhythmOf(lChase.Camera).bWarned);
    }
}

TEST_CASE("Camera rhythm: a camera moved both in the steps and in Update is 'mixed' and warns, at any frame rate")
{
    for (const StepPattern& lPattern : PATTERNS)
    {
        CAPTURE(lPattern.Name);

        // Chased in the steps, and shaken in Update.
        ChaseWorld lChase(EFollow::Chased);
        PlaySteps(lChase, lPattern, THREE_WINDOWS, [&lChase](const Uint32 InFrame)
        {
            lChase.Camera.Get<TransformComponent>().Position.y += (InFrame % 2 == 0) ? 3.f : -2.f;
        });

        const CameraRhythm& lRhythm = RhythmOf(lChase.Camera);
        CHECK(lRhythm.Last == ECameraRhythm::Mixed);
        CHECK(lRhythm.bWarned);
        CHECK(lRhythm.MovedBoth == lChase.Camera.GetHandle());
        CHECK_FALSE(lRhythm.bSizeBoth);
    }
}

TEST_CASE("Camera rhythm: the entity moved both ways is found up the chain; a rig moved in the steps with the camera offset in Update is not")
{
    for (const StepPattern& lPattern : PATTERNS)
    {
        CAPTURE(lPattern.Name);

        // The body the camera hangs from runs in the steps and is nudged in Update.
        {
            ChaseWorld lChase(EFollow::Parented);
            TransformInterpolation::RecordSteps(lChase.Body);
            PlaySteps(lChase, lPattern, THREE_WINDOWS, [&lChase](Uint32) { lChase.Body.Get<TransformComponent>().Position.y += 1.f; });

            CHECK(RhythmOf(lChase.Camera).Last == ECameraRhythm::Mixed);
            CHECK(RhythmOf(lChase.Camera).MovedBoth == lChase.Body.GetHandle());
        }

        // The body runs in the steps, the camera under it is offset in Update: each moves one way.
        {
            ChaseWorld lChase(EFollow::Parented);
            TransformInterpolation::RecordSteps(lChase.Body);
            PlaySteps(lChase, lPattern, THREE_WINDOWS, [&lChase](const Uint32 InFrame)
            {
                lChase.Camera.Get<TransformComponent>().Position.y = CAMERA_OFFSET.y + static_cast<float>(InFrame % 3);
            });

            CHECK(RhythmOf(lChase.Camera).Last != ECameraRhythm::Mixed);
            CHECK_FALSE(RhythmOf(lChase.Camera).bWarned);
        }
    }
}

TEST_CASE("Camera rhythm: a size changed both in the steps and in Update is 'mixed', the size named")
{
    for (const StepPattern& lPattern : PATTERNS)
    {
        CAPTURE(lPattern.Name);

        ChaseWorld lChase(EFollow::Chased);
        lChase.ZoomEachStep(ZOOM_PER_STEP);
        PlaySteps(lChase, lPattern, THREE_WINDOWS, [&lChase](Uint32) { lChase.Camera.Get<CameraComponent>().OrthoSize -= 1.f; });

        const CameraRhythm& lRhythm = RhythmOf(lChase.Camera);
        CHECK(lRhythm.Last == ECameraRhythm::Mixed);
        CHECK(lRhythm.MovedBoth == lChase.Camera.GetHandle());
        CHECK(lRhythm.bSizeBoth);
    }
}

TEST_CASE("Camera rhythm: a respawn now and then, or a hand-over from Update to the steps, is never 'mixed'")
{
    for (const StepPattern& lPattern : PATTERNS)
    {
        CAPTURE(lPattern.Name);

        // Chased, and put elsewhere in Update without a cut every 40 frames.
        {
            ChaseWorld lChase(EFollow::Chased);
            Uint32     lSteps = 0;
            for (Uint32 lFrame = 0; lSteps < THREE_WINDOWS; ++lFrame)
            {
                if (lFrame % 40 == 39)
                {
                    lChase.Camera.Get<TransformComponent>().Position.x -= 500.f;
                }
                const Uint32 lFrameSteps = lPattern.Steps[lFrame % lPattern.Steps.size()];
                lChase.Tick(lFrameSteps);
                lSteps += lFrameSteps;
                CHECK(RhythmOf(lChase.Camera).Last != ECameraRhythm::Mixed);
            }
            CHECK(RhythmOf(lChase.Camera).Last == ECameraRhythm::Steps);
        }

        // Driven in Update for a while, then chased in the steps.
        {
            ChaseWorld            lChase(EFollow::Chased);
            ChaseCameraSubsystem* lChaser = lChase.TheWorld->GetSubsystems().GetSubsystem<ChaseCameraSubsystem>();
            lChaser->Camera = ENTITY_NONE;
            PlaySteps(lChase, lPattern, THREE_WINDOWS, [&lChase](Uint32) { lChase.Camera.Get<TransformComponent>().Position.x += 4.f; });
            CHECK(RhythmOf(lChase.Camera).Last == ECameraRhythm::Update);

            lChaser->Camera = lChase.Camera.GetHandle();
            PlaySteps(lChase, lPattern, THREE_WINDOWS, [](Uint32) {});
            CHECK(RhythmOf(lChase.Camera).Last == ECameraRhythm::Steps);
            CHECK_FALSE(RhythmOf(lChase.Camera).bWarned);
        }
    }
}
