// Suite: CameraDirector — the hand-over from one camera to the next. A camera raised in a fixed step
// is blended to from that step's start, so every frame shows the same view at the same game time
// whatever the frames (zero to three steps, any alpha); a blend interrupted goes on under the next;
// a cut ends it; a camera raised between steps starts at the last step's end, and a paused world
// holds still. Driven through WorldManager the way Engine::Loop runs a frame, the director asked
// before each frame is framed, as CameraManager::Render does.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Core/Templates/TypeTraits.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/Camera/CameraBlend.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraDirector.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    constexpr double STEP_SECONDS = 1.0 / 60.0;

    /** The chased camera's runner: 30 units a step. */
    constexpr float SPEED = 1800.f;

    const Vector2F CHASE_OFFSET{ 0.f, 40.f };

    // Puts the chased camera at the runner's place every step, the runner moving at SPEED from 0.
    class DirectorChaser : public WorldSubsystem
    {
        OPAAX_OBJECT(DirectorChaser, WorldSubsystem)

    public:
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Presentation; }

        void FixedUpdate(const double InFixedDeltaTime) override
        {
            m_Runner += SPEED * static_cast<float>(InFixedDeltaTime);

            Entity lCamera{ Camera, &GetWorld() };
            if (lCamera.IsValid())
            {
                lCamera.Get<TransformComponent>().Position = Vector2F{ m_Runner, 0.f } + CHASE_OFFSET;
            }
        }

        EntityID Camera = ENTITY_NONE;

    private:
        float m_Runner = 0.f;
    };

    // Raises cameras in a fixed step, as gameplay does: each the step after the world counted its step.
    class DirectorRaiser : public WorldSubsystem
    {
        OPAAX_OBJECT(DirectorRaiser, WorldSubsystem)

    public:
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Gameplay; }

        void FixedUpdate(double) override
        {
            for (const Raise& lRaise : Raises)
            {
                Entity lCamera{ lRaise.Camera, &GetWorld() };
                if (GetWorld().GetStepCount() == lRaise.AtStep && lCamera.IsValid())
                {
                    lCamera.Get<CameraComponent>().Priority = lRaise.Priority;
                }
            }
        }

        struct Raise
        {
            EntityID Camera   = ENTITY_NONE;
            Uint64   AtStep   = 0;
            Int32    Priority = 0;
        };
        TDynArray<Raise> Raises;
    };

    // Moves the view by Offset.
    class NudgeModifier final : public CameraModifier
    {
        OPAAX_OBJECT(NudgeModifier, CameraModifier)

    public:
        Vector2F Offset = { 0.f, 0.f };

        void Modify(const CameraModifierContext&, CameraView& InOutView) const override { InOutView.Position += Offset; }
    };

    // Doubles the view's distance from the origin.
    class DoubleModifier final : public CameraModifier
    {
        OPAAX_OBJECT(DoubleModifier, CameraModifier)

    public:
        void Modify(const CameraModifierContext&, CameraView& InOutView) const override { InOutView.Position *= 2.f; }
    };

    // Sways the view sideways with game time, as a shake does: 10 units at 1 Hz.
    class SwayModifier final : public CameraModifier
    {
        OPAAX_OBJECT(SwayModifier, CameraModifier)

    public:
        static float At(const double InTime) { return 10.f * static_cast<float>(Maths::Sin(2.0 * DPI * InTime)); }

        void Modify(const CameraModifierContext& InContext, CameraView& InOutView) const override
        {
            InOutView.Position.x += At(InContext.GameTime);
        }
    };

    // Breaks the view, as a division by zero in a modifier would.
    class BrokenModifier final : public CameraModifier
    {
        OPAAX_OBJECT(BrokenModifier, CameraModifier)

    public:
        void Modify(const CameraModifierContext&, CameraView& InOutView) const override
        {
            InOutView.OrthoSize = TNumericLimits<float>::quiet_NaN();
        }
    };

    // A Play world with a director: Chase (chased, priority 1), Wide and Close (fixed, below it).
    struct DirectorWorld
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;

        World*          TheWorld = nullptr;
        CameraDirector* Director = nullptr;
        Entity          Chase;
        Entity          Wide;
        Entity          Close;

        /** The views Wide and Close give: they do not move. */
        static inline const CameraView WIDE_VIEW{ { 600.f, 100.f }, 450.f, 20.f };
        static inline const CameraView CLOSE_VIEW{ { -200.f, -50.f }, 200.f, 0.f };

        DirectorWorld()
        {
            TheWorld = Worlds.CreateWorld("Director", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events, Input, Config,
                                               /*Actions*/ nullptr, /*UI*/ nullptr, Debug });
            TheWorld->GetSubsystems().Add<DirectorRaiser>();
            TheWorld->GetSubsystems().Add<DirectorChaser>();
            TheWorld->GetSubsystems().Add<CameraDirector>();
            TheWorld->GetSubsystems().StartupAll();

            Chase = TheWorld->CreateEntity("Chase");
            Chase.Add<CameraComponent>().Priority = 1;
            Chase.Get<TransformComponent>().Position = CHASE_OFFSET;

            Wide = MakeFixed("Wide", WIDE_VIEW);
            Wide.Get<CameraComponent>().BlendIn    = 0.5f;
            Wide.Get<CameraComponent>().BlendCurve = ECameraBlend::Linear;

            Close = MakeFixed("Close", CLOSE_VIEW);
            Close.Get<CameraComponent>().BlendIn    = 0.25f;
            Close.Get<CameraComponent>().BlendCurve = ECameraBlend::EaseInOut;

            TheWorld->GetSubsystems().GetSubsystem<DirectorChaser>()->Camera = Chase.GetHandle();
            Director = TheWorld->GetSubsystems().GetSubsystem<CameraDirector>();
            REQUIRE(Director != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        Entity MakeFixed(const char* InName, const CameraView& InView)
        {
            Entity lCamera = TheWorld->CreateEntity(InName);
            CameraComponent& lSettings = lCamera.Add<CameraComponent>();
            lSettings.OrthoSize   = InView.OrthoSize;
            lSettings.bRotateView = true;
            lCamera.Get<TransformComponent>().Position = InView.Position;
            lCamera.Get<TransformComponent>().Rotation = InView.Rotation;
            return lCamera;
        }

        /** InCamera raised to InPriority in the step after the world's InAtStep-th. */
        void RaiseInStep(const Entity InCamera, const Uint64 InAtStep, const Int32 InPriority)
        {
            TheWorld->GetSubsystems().GetSubsystem<DirectorRaiser>()->Raises.push_back({ InCamera.GetHandle(), InAtStep, InPriority });
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

        /** The view a frame drawn at InAlpha shows, as CameraManager::Render frames it. */
        CameraResolution Draw(const float InAlpha)
        {
            Director->Direct();
            return CameraManager::Resolve(*TheWorld, InAlpha);
        }

        double GameTime(const float InAlpha) const { return CameraDirector::GameTimeAt(*TheWorld, InAlpha); }

        /** Where Chase is drawn at game time InTime: on the runner, which moves at SPEED from 0. */
        static CameraView ChaseAt(const double InTime)
        {
            return CameraView{ Vector2F{ SPEED * static_cast<float>(InTime), 0.f } + CHASE_OFFSET, 300.f, 0.f };
        }
    };

    void CheckView(const CameraView& InView, const CameraView& InExpected)
    {
        CHECK(InView.Position.x == doctest::Approx(InExpected.Position.x).epsilon(1e-5));
        CHECK(InView.Position.y == doctest::Approx(InExpected.Position.y).epsilon(1e-5));
        CHECK(InView.OrthoSize == doctest::Approx(InExpected.OrthoSize).epsilon(1e-5));
        CHECK(InView.Rotation == doctest::Approx(InExpected.Rotation).epsilon(1e-5));
    }

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

TEST_CASE("CameraDirector: a camera raised in a step is blended to from that step's start, the same at any frames")
{
    // Wide raised in step 21 (from 20 steps of time), Close in step 41, while Wide's blend runs.
    constexpr double WIDE_AT  = 20 * STEP_SECONDS;
    constexpr double CLOSE_AT = 40 * STEP_SECONDS;

    // The view at a game time, from the cameras' paths: the chase moving, the blends on their clocks.
    const auto lExpected = [](const double InTime)
    {
        const float lWide  = CameraBlendWeight(ECameraBlend::Linear, static_cast<float>((InTime - WIDE_AT) / 0.5));
        const float lClose = CameraBlendWeight(ECameraBlend::EaseInOut, static_cast<float>((InTime - CLOSE_AT) / 0.25));
        CameraView  lView  = DirectorWorld::ChaseAt(InTime);
        if (InTime > WIDE_AT)
        {
            lView = BlendCameraViews(lView, DirectorWorld::WIDE_VIEW, lWide);
        }
        if (InTime > CLOSE_AT)
        {
            lView = BlendCameraViews(lView, DirectorWorld::CLOSE_VIEW, lClose);
        }
        return lView;
    };

    // Frames of 0 to 3 steps, or one step each at a fixed alpha: two clocks, one path.
    for (const bool bSteady : { false, true })
    {
        CAPTURE(bSteady);
        DirectorWorld lWorld;
        lWorld.RaiseInStep(lWorld.Wide, 20, 5);
        lWorld.RaiseInStep(lWorld.Close, 40, 9);

        lWorld.Tick(1);
        Uint32 lFrames = 0;
        while (lWorld.TheWorld->GetStepCount() < 80)
        {
            const Frame lFrame = bSteady ? Frame{ 1, 0.37f } : FRAMES[lFrames % 10];
            ++lFrames;

            lWorld.Tick(lFrame.Steps);
            const double           lTime       = lWorld.GameTime(lFrame.Alpha);
            const CameraResolution lResolution = lWorld.Draw(lFrame.Alpha);

            CAPTURE(lTime);
            CheckView(lResolution.View, lExpected(lTime));
        }

        // Both blends over: Close alone, its blend dropped.
        CHECK(lWorld.Director->GetLive() == lWorld.Close.GetHandle());
        CHECK(lWorld.Director->GetShots().size() == 1);
    }
}

TEST_CASE("CameraDirector: a blend starts on the old view exactly and ends on the new one exactly")
{
    DirectorWorld lWorld;
    lWorld.RaiseInStep(lWorld.Wide, 10, 5);

    lWorld.Tick(11);

    // The frame at the start of the step it was raised in: still the chase, the blend at 0.
    const CameraResolution lStart = lWorld.Draw(0.f);
    CHECK(lStart.Entity == lWorld.Wide.GetHandle());
    CHECK(lStart.Blends == 1);
    CHECK(lStart.BlendFrom == lWorld.Chase.GetHandle());
    CHECK(lStart.BlendWeight == doctest::Approx(0.f));
    CheckView(lStart.View, DirectorWorld::ChaseAt(10 * STEP_SECONDS));

    // Past half a second later: Wide exactly, and the chase dropped (the blend ended before the last step).
    lWorld.Tick(31);
    const CameraResolution lEnd = lWorld.Draw(0.f);
    CHECK(lEnd.BlendWeight == 1.f);
    CHECK(lEnd.View.Position.x == DirectorWorld::WIDE_VIEW.Position.x);
    CHECK(lEnd.View.OrthoSize == DirectorWorld::WIDE_VIEW.OrthoSize);
    CHECK(lEnd.View.Rotation == DirectorWorld::WIDE_VIEW.Rotation);
    CHECK(lEnd.Blends == 0);
    CHECK(lWorld.Director->GetShots().size() == 1);
}

TEST_CASE("CameraDirector: a cut ends the blend on the live camera; BlendIn 0 is a cut")
{
    DirectorWorld lWorld;
    lWorld.RaiseInStep(lWorld.Wide, 5, 5);
    lWorld.Tick(15);
    REQUIRE(lWorld.Draw(0.5f).Blends == 1);

    CameraManager::Cut(*lWorld.TheWorld);
    const CameraResolution lCut = lWorld.Draw(0.5f);
    CHECK(lCut.Blends == 0);
    CHECK(lCut.View.Position.x == DirectorWorld::WIDE_VIEW.Position.x);

    // Close takes over at once.
    lWorld.Close.Get<CameraComponent>().BlendIn  = 0.f;
    lWorld.Close.Get<CameraComponent>().Priority = 9;
    const CameraResolution lClose = lWorld.Draw(0.5f);
    CHECK(lClose.Entity == lWorld.Close.GetHandle());
    CHECK(lClose.Blends == 0);
    CHECK(lClose.View.Position.x == DirectorWorld::CLOSE_VIEW.Position.x);
}

TEST_CASE("CameraDirector: a camera raised between steps starts at the last step's end; a paused world holds")
{
    DirectorWorld lWorld;
    lWorld.Tick(10);
    lWorld.Draw(0.5f);

    // Raised by a tool between frames: the frames before the next step still show the chase.
    lWorld.Wide.Get<CameraComponent>().Priority = 5;
    const double lRaisedAt = lWorld.TheWorld->GetSteppedTime();
    for (const float lAlpha : { 0.5f, 0.7f, 0.99f })
    {
        const CameraResolution lBefore = lWorld.Draw(lAlpha);
        CHECK(lBefore.Entity == lWorld.Wide.GetHandle());
        CHECK(lBefore.BlendWeight == 0.f);
        CheckView(lBefore.View, DirectorWorld::ChaseAt(lWorld.GameTime(lAlpha)));
    }

    // A paused world (no step, the same alpha) draws the same view, frame after frame.
    lWorld.Tick(1);
    const CameraView lHeld = lWorld.Draw(0.4f).View;
    for (Uint32 lFrame = 0; lFrame < 5; ++lFrame)
    {
        lWorld.Tick(0);
        CheckView(lWorld.Draw(0.4f).View, lHeld);
    }

    // On its clock from the last step's end.
    const CameraResolution lAfter = lWorld.Draw(0.4f);
    const double           lTime  = lWorld.GameTime(0.4f);
    CHECK(lAfter.BlendWeight == doctest::Approx((lTime - lRaisedAt) / 0.5));
    CheckView(lAfter.View, BlendCameraViews(DirectorWorld::ChaseAt(lTime), DirectorWorld::WIDE_VIEW,
                                            static_cast<float>((lTime - lRaisedAt) / 0.5)));
}

TEST_CASE("CameraDirector: a camera destroyed while the view leaves it is held where it was last seen")
{
    DirectorWorld lWorld;
    lWorld.RaiseInStep(lWorld.Wide, 5, 5);
    lWorld.Tick(10);
    lWorld.Draw(0.f);

    const Vector2F lLastSeen = lWorld.Chase.Get<TransformComponent>().Position;
    lWorld.TheWorld->DestroyEntity(lWorld.Chase);

    const CameraResolution lResolution = lWorld.Draw(0.f);
    CHECK(lResolution.Blends == 1);
    CHECK(lResolution.BlendFrom == ENTITY_NONE);
    CheckView(lResolution.View, BlendCameraViews(CameraView{ lLastSeen, 300.f, 0.f }, DirectorWorld::WIDE_VIEW,
                                                 lResolution.BlendWeight));
}

TEST_CASE("Camera: a camera saved before BlendIn cuts, and its blend round-trips")
{
    const CameraComponent lOld = nlohmann::json{ { "OrthoSize", 120.f }, { "Priority", 2 } }.get<CameraComponent>();
    CHECK(lOld.BlendIn == 0.f);
    CHECK(lOld.BlendCurve == ECameraBlend::EaseInOut);

    CameraComponent lCamera;
    lCamera.BlendIn    = 1.25f;
    lCamera.BlendCurve = ECameraBlend::EaseOut;
    const CameraComponent lBack = nlohmann::json(lCamera).get<CameraComponent>();
    CHECK(lBack.BlendIn == 1.25f);
    CHECK(lBack.BlendCurve == ECameraBlend::EaseOut);
}

TEST_CASE("CameraDirector: modifiers change the blended view by priority; the view before them is kept")
{
    DirectorWorld lWorld;
    lWorld.Tick(5);

    lWorld.Director->AddModifier<NudgeModifier>().Offset = Vector2F{ 10.f, 0.f };
    lWorld.Director->GetModifiers().back()->Priority     = 0;
    DoubleModifier& lDouble = lWorld.Director->AddModifier<DoubleModifier>();
    REQUIRE(lWorld.Director->GetModifiers().size() == 2);

    // Added later at the same priority: runs after the nudge.
    const CameraView       lChase = DirectorWorld::ChaseAt(lWorld.GameTime(0.5f));
    const CameraResolution lBoth  = lWorld.Draw(0.5f);
    CheckView(lBoth.Unmodified, lChase);
    CHECK(lBoth.View.Position.x == doctest::Approx((lChase.Position.x + 10.f) * 2.f));

    // A lower priority runs first.
    lWorld.Director->RemoveModifier(lDouble);
    lWorld.Director->AddModifier<DoubleModifier>().Priority = -1;
    REQUIRE(lWorld.Director->GetModifiers().size() == 2);
    CHECK(lWorld.Draw(0.5f).View.Position.x == doctest::Approx(lChase.Position.x * 2.f + 10.f));

    // The camera itself, as a preview shows it, is not modified.
    CheckView(CameraManager::ResolveCamera(lWorld.Chase, 0.5f), lChase);
}

TEST_CASE("CameraDirector: a modifier of game time draws the same at any frames, and holds when paused")
{
    for (const bool bSteady : { false, true })
    {
        CAPTURE(bSteady);
        DirectorWorld lWorld;
        lWorld.Director->AddModifier<SwayModifier>();
        lWorld.Tick(1);

        for (Uint32 lFrame = 0; lFrame < 40; ++lFrame)
        {
            const Frame lPaced = bSteady ? Frame{ 1, 0.37f } : FRAMES[lFrame % 10];
            lWorld.Tick(lPaced.Steps);

            const double lTime     = lWorld.GameTime(lPaced.Alpha);
            CameraView   lExpected = DirectorWorld::ChaseAt(lTime);
            lExpected.Position.x  += SwayModifier::At(lTime);
            CAPTURE(lTime);
            CheckView(lWorld.Draw(lPaced.Alpha).View, lExpected);
        }

        // No step, the same alpha: the same view.
        const CameraView lHeld = lWorld.Draw(0.2f).View;
        lWorld.Tick(0);
        CheckView(lWorld.Draw(0.2f).View, lHeld);
    }
}

TEST_CASE("CameraDirector: a view a modifier breaks is caught by the guard")
{
    DirectorWorld lWorld;
    lWorld.Tick(2);
    const CameraView lGood = lWorld.Draw(0.5f).View;

    lWorld.Director->AddModifier<BrokenModifier>();
    CameraView lBroken = lWorld.Draw(0.5f).View;
    CHECK(CameraManager::GuardView(lBroken, lGood) == EViewGuard::Replaced);
    CheckView(lBroken, lGood);
}
