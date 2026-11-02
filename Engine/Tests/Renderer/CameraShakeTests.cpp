// Suite: camera shakes — the CameraShake asset (its fades, its pattern of time and seed), attenuation,
// trauma, and CameraShakeSubsystem: a shake played in a fixed step is drawn from that step's start,
// the same at any frames (zero to three steps, any alpha), and holds when paused; one played between
// steps starts at the last step's end; a source is attenuated by the view's distance and stops with
// its entity; a trauma source shakes at its trauma squared. Driven through WorldManager the way
// Engine::Loop runs a frame, the director asked before each frame, as CameraManager::Render does.
#include <doctest.h>

#include "../TestFolderPaths.h"
#include "../TestTempDir.h"

#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Core/Templates/TypeTraits.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraDirector.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/CameraShake.h"
#include "Renderer/Camera/CameraShakeSourceComponent.h"
#include "Renderer/Camera/CameraShakeSubsystem.h"
#include "Renderer/DebugDraw.h"
#include "Resources/Asset/AssetFile.h"
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

    struct ShakeClasses final : ObjectClassRegistry
    {
        ShakeClasses() { Register<CameraShake>(); }
    };

    // Runs a cue in a fixed step, as gameplay does: each the step after the world counted its step.
    class ShakeCues : public WorldSubsystem
    {
        OPAAX_OBJECT(ShakeCues, WorldSubsystem)

    public:
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Gameplay; }

        void FixedUpdate(double) override
        {
            for (const Cue& lCue : Cues)
            {
                if (GetWorld().GetStepCount() == lCue.AtStep)
                {
                    lCue.Run(*GetWorld().GetSubsystems().GetSubsystem<CameraShakeSubsystem>());
                }
            }
        }

        struct Cue
        {
            Uint64                                  AtStep = 0;
            TFunction<void(CameraShakeSubsystem&)> Run;
        };
        TDynArray<Cue> Cues;
    };

    // A Play world with a director and shakes, framed by one camera standing at the origin: the view a
    // frame draws is the shakes alone.
    struct ShakeWorld
    {
        TestTempDir      Dir{ "cameraShake" };
        ShakeClasses     Classes;
        TestFolderPaths  Paths{ Dir.Path() };
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;

        World*                TheWorld = nullptr;
        CameraShakeSubsystem* Shakes   = nullptr;
        CameraDirector*       Director = nullptr;

        ShakeWorld()
        {
            Resources.SetObjectClasses(&Classes);
            REQUIRE(Resources.Startup());

            TheWorld = Worlds.CreateWorld("Shake", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, Paths, Events, Input, Config,
                                               /*Actions*/ nullptr, /*UI*/ nullptr, Debug });
            TheWorld->GetSubsystems().Add<ShakeCues>();
            TheWorld->GetSubsystems().Add<CameraDirector>();
            TheWorld->GetSubsystems().Add<CameraShakeSubsystem>();
            TheWorld->GetSubsystems().StartupAll();

            TheWorld->CreateEntity("Camera").Add<CameraComponent>().Priority = 1;

            Shakes   = TheWorld->GetSubsystems().GetSubsystem<CameraShakeSubsystem>();
            Director = TheWorld->GetSubsystems().GetSubsystem<CameraDirector>();
            REQUIRE(Shakes != nullptr);
            REQUIRE(Director != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        ~ShakeWorld()
        {
            Worlds.Shutdown();
            Resources.Shutdown();
        }

        /** InShake saved as InName under the world's asset folder. */
        void Save(const char* InName, const CameraShake& InShake) const
        {
            REQUIRE(AssetFile::Save(Dir.Sub(InName), InShake));
        }

        /** InRun in the step after the world's InAtStep-th. */
        void Cue(const Uint64 InAtStep, TFunction<void(CameraShakeSubsystem&)> InRun)
        {
            TheWorld->GetSubsystems().GetSubsystem<ShakeCues>()->Cues.push_back({ InAtStep, Move(InRun) });
        }

        /** A source of InShakePath at InPosition. */
        Entity MakeSource(const char* InShakePath, const Vector2F InPosition)
        {
            Entity lSource = TheWorld->CreateEntity("Source");
            lSource.Get<TransformComponent>().Position = InPosition;
            lSource.Add<CameraShakeSourceComponent>().Shake.Path = OpaaxString(InShakePath);
            return lSource;
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
        CameraView Draw(const float InAlpha)
        {
            Director->Direct();
            return CameraManager::Resolve(*TheWorld, InAlpha).View;
        }

        double GameTime(const float InAlpha) const { return CameraDirector::GameTimeAt(*TheWorld, InAlpha); }
    };

    /** A shake made in code. */
    TStrongObjectPtr<CameraShake> MakeShake(const ECameraShakePattern InPattern, const float InDuration)
    {
        TStrongObjectPtr<CameraShake> lShake = NewObject<CameraShake>();
        lShake->Pattern       = InPattern;
        lShake->Amplitude     = 12.f;
        lShake->RollAmplitude = 2.f;
        lShake->Frequency     = 9.f;
        lShake->Duration      = InDuration;
        lShake->BlendIn       = 0.1f;
        lShake->BlendOut      = 0.3f;
        return lShake;
    }

    /** The view of the camera at the origin, moved by InOffset times InScale. */
    void CheckShaken(const CameraView& InView, const CameraShakeOffset& InOffset, const float InScale)
    {
        CHECK(InView.Position.x == doctest::Approx(InOffset.Position.x * InScale).epsilon(1e-5));
        CHECK(InView.Position.y == doctest::Approx(InOffset.Position.y * InScale).epsilon(1e-5));
        CHECK(InView.Rotation == doctest::Approx(InOffset.Rotation * InScale).epsilon(1e-5));
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

// =============================================================================
// The asset
// =============================================================================

TEST_CASE("CameraShake: fades in, holds, and fades out to nothing at its end, or once stopped")
{
    CameraShake lShake;
    lShake.Duration = 1.f;
    lShake.BlendIn  = 0.2f;
    lShake.BlendOut = 0.4f;

    CHECK(lShake.Envelope(-0.01, {}) == 0.f);
    CHECK(lShake.Envelope(0.0, {}) == 0.f);
    CHECK(lShake.Envelope(0.1, {}) == doctest::Approx(0.5f));
    CHECK(lShake.Envelope(0.2, {}) == 1.f);
    CHECK(lShake.Envelope(0.6, {}) == 1.f);
    CHECK(lShake.Envelope(0.8, {}) == doctest::Approx(0.5f));
    CHECK(lShake.Envelope(0.999, {}) < 0.001f);
    CHECK(lShake.Envelope(1.0, {}) == 0.f);
    CHECK(lShake.EndTime({}) == doctest::Approx(1.0));

    // Stopped: fades out over BlendOut from there, never above where it was.
    CHECK(lShake.EndTime(0.3) == doctest::Approx(0.7));
    CHECK(lShake.Envelope(0.5, 0.3) == doctest::Approx(0.5f));
    CHECK(lShake.Envelope(0.7, 0.3) < 1e-6f);
    CHECK(lShake.Envelope(0.71, 0.3) == 0.f);
    CHECK(lShake.Envelope(0.05, 0.05) == doctest::Approx(lShake.Envelope(0.05, {})));

    // Endless until stopped.
    lShake.Duration = 0.f;
    CHECK(lShake.EndTime({}) == TNumericLimits<double>::infinity());
    CHECK(lShake.Envelope(100.0, {}) == 1.f);
    CHECK(lShake.Envelope(100.2, 100.0) == doctest::Approx(0.5f));

    // Nothing outside its time.
    const CameraShakeOffset lAfter = lShake.Sample(1, 101.0, 100.0);
    CHECK(lAfter.Position == Vector2F(0.f, 0.f));
    CHECK(lAfter.Rotation == 0.f);
}

TEST_CASE("CameraShake: its offset depends on the time and the seed alone, within its amplitudes")
{
    for (const ECameraShakePattern lPattern : { ECameraShakePattern::Noise, ECameraShakePattern::Wave })
    {
        CAPTURE(ToString(lPattern));
        const TStrongObjectPtr<CameraShake> lShake = MakeShake(lPattern, 0.f);

        // Read forward, then backward: the same values, so a frame never depends on the frames before it.
        TDynArray<CameraShakeOffset> lForward;
        for (Uint32 lIndex = 0; lIndex < 200; ++lIndex)
        {
            lForward.push_back(lShake->Oscillate(7, lIndex * 0.0137));
        }

        bool bMoved = false;
        for (Uint32 lIndex = 200; lIndex-- > 0;)
        {
            const CameraShakeOffset lAgain = lShake->Oscillate(7, lIndex * 0.0137);
            CHECK(lAgain.Position == lForward[lIndex].Position);
            CHECK(lAgain.Rotation == lForward[lIndex].Rotation);
            CHECK(Maths::Abs(lAgain.Position.x) <= 12.f);
            CHECK(Maths::Abs(lAgain.Position.y) <= 12.f);
            CHECK(Maths::Abs(lAgain.Rotation) <= 2.f);
            bMoved = bMoved || Maths::Abs(lAgain.Position.x) > 3.f;
        }
        CHECK(bMoved);

        // Smooth: a tenth of a millisecond moves it little.
        const CameraShakeOffset lAt   = lShake->Oscillate(7, 0.42);
        const CameraShakeOffset lNext = lShake->Oscillate(7, 0.4201);
        CHECK(Maths::Abs(lNext.Position.x - lAt.Position.x) < 0.1f);
        CHECK(Maths::Abs(lNext.Position.y - lAt.Position.y) < 0.1f);

        // Another seed, another shake.
        CHECK(lShake->Oscillate(8, 0.42).Position != lAt.Position);
    }

    // A wave repeats at its frequency.
    const TStrongObjectPtr<CameraShake> lWave = MakeShake(ECameraShakePattern::Wave, 0.f);
    CHECK(lWave->Oscillate(3, 0.25).Position.x == doctest::Approx(lWave->Oscillate(3, 0.25 + 1.0 / 9.0).Position.x).epsilon(1e-4));
}

TEST_CASE("CameraShake: numbers below 0 are raised to 0, and it round-trips through its file")
{
    CameraShake lShake;
    lShake.Amplitude = -4.f;
    lShake.Duration  = -1.f;
    lShake.BlendOut  = -0.5f;
    lShake.PostEditChange();
    CHECK(lShake.Amplitude == 0.f);
    CHECK(lShake.Duration == 0.f);
    CHECK(lShake.BlendOut == 0.f);

    const TestTempDir  lDir("cameraShakeFile");
    const ShakeClasses lClasses;

    CameraShake lSaved;
    lSaved.Pattern       = ECameraShakePattern::Wave;
    lSaved.Amplitude     = 30.f;
    lSaved.RollAmplitude = 4.f;
    lSaved.Duration      = 0.f;
    REQUIRE(AssetFile::Save(lDir.Sub("Rumble.opaaxasset"), lSaved));

    const TStrongObjectPtr<Asset> lLoaded = AssetFile::LoadAsset(lDir.Sub("Rumble.opaaxasset"), lClasses);
    const CameraShake* lBack = Cast<CameraShake>(lLoaded.Get());
    REQUIRE(lBack != nullptr);
    CHECK(lBack->Pattern == ECameraShakePattern::Wave);
    CHECK(lBack->Amplitude == 30.f);
    CHECK(lBack->RollAmplitude == 4.f);
    CHECK(lBack->Duration == 0.f);
    CHECK(lBack->Frequency == CameraShake{}.Frequency);
}

TEST_CASE("CameraShakeAttenuation: all within the inner radius, none from the outer, linear between")
{
    CHECK(CameraShakeAttenuation(50.f, 100.f, 500.f) == 1.f);
    CHECK(CameraShakeAttenuation(300.f, 100.f, 500.f) == doctest::Approx(0.5f));
    CHECK(CameraShakeAttenuation(500.f, 100.f, 500.f) == 0.f);
    CHECK(CameraShakeAttenuation(9000.f, 100.f, 0.f) == 1.f);
}

TEST_CASE("CameraTrauma: falls linearly, kept in [0, 1]; a frame drawn before a hit sees the level before it")
{
    CameraTrauma lTrauma;
    lTrauma.Decay = 0.5f;

    lTrauma.Add(0.8f, 1.0);
    CHECK(lTrauma.At(1.0) == doctest::Approx(0.8f));
    CHECK(lTrauma.At(1.4) == doctest::Approx(0.6f));
    CHECK(lTrauma.At(1.8) == doctest::Approx(0.4f));
    CHECK(lTrauma.At(3.0) == 0.f);

    lTrauma.Add(0.5f, 2.0);
    CHECK(lTrauma.At(2.0) == doctest::Approx(0.8f));
    CHECK(lTrauma.At(1.9) == doctest::Approx(0.35f));

    // Twice at one time (two hits in a step): one level, at most 1.
    lTrauma.Add(0.5f, 2.0);
    CHECK(lTrauma.At(2.0) == 1.f);
    CHECK(lTrauma.At(1.9) == doctest::Approx(0.35f));
}

TEST_CASE("CameraShakeSource: a source saved before a field has its default, and it round-trips")
{
    const CameraShakeSourceComponent lOld = nlohmann::json{ { "Scale", 2.f } }.get<CameraShakeSourceComponent>();
    CHECK(lOld.Scale == 2.f);
    CHECK(lOld.bPlayOnStart);
    CHECK_FALSE(lOld.bUseTrauma);
    CHECK(lOld.OuterRadius == 0.f);

    CameraShakeSourceComponent lSource;
    lSource.Shake.Path  = OpaaxString("Shakes/Hit.opaaxasset");
    lSource.Seed        = 9;
    lSource.bUseTrauma  = true;
    lSource.TraumaDecay = 0.25f;
    const CameraShakeSourceComponent lBack = nlohmann::json(lSource).get<CameraShakeSourceComponent>();
    CHECK(lBack.Shake == lSource.Shake);
    CHECK(lBack.Seed == 9);
    CHECK(lBack.bUseTrauma);
    CHECK(lBack.TraumaDecay == 0.25f);
}

// =============================================================================
// Playing
// =============================================================================

TEST_CASE("CameraShakeSubsystem: a shake played in a step is drawn from its start the same at any frames, and holds when paused")
{
    constexpr Uint64 PLAY_STEP = 10;
    constexpr double PLAYED_AT = PLAY_STEP * STEP_SECONDS;

    for (const bool bSteady : { false, true })
    {
        CAPTURE(bSteady);
        ShakeWorld lWorld;
        const TStrongObjectPtr<CameraShake> lShake = MakeShake(ECameraShakePattern::Noise, 1.f);
        lWorld.Cue(PLAY_STEP, [&lShake](CameraShakeSubsystem& InShakes) { InShakes.Play(lShake, 1.5f, 4); });

        lWorld.Tick(1);
        for (Uint32 lFrame = 0; lWorld.TheWorld->GetStepCount() < 80; ++lFrame)
        {
            const Frame lPaced = bSteady ? Frame{ 1, 0.37f } : FRAMES[lFrame % 10];
            lWorld.Tick(lPaced.Steps);

            const double lTime = lWorld.GameTime(lPaced.Alpha);
            CAPTURE(lTime);
            CheckShaken(lWorld.Draw(lPaced.Alpha), lShake->Sample(4, lTime - PLAYED_AT, {}), 1.5f);

            // No step, the same alpha: the same view.
            if (lPaced.Steps == 0)
            {
                const CameraView lHeld = lWorld.Draw(lPaced.Alpha);
                lWorld.Tick(0);
                CheckShaken(lWorld.Draw(lPaced.Alpha), CameraShakeOffset{ lHeld.Position, lHeld.Rotation }, 1.f);
            }
        }

        // Over for every frame still to come: let go.
        CHECK(lWorld.Shakes->GetPlaying().empty());
    }
}

TEST_CASE("CameraShakeSubsystem: a shake played or stopped between steps counts from the last step's end")
{
    ShakeWorld lWorld;
    const TStrongObjectPtr<CameraShake> lShake = MakeShake(ECameraShakePattern::Wave, 0.f);
    lWorld.Tick(3);

    const double lStart = lWorld.TheWorld->GetSteppedTime();
    const Uint32 lId    = lWorld.Shakes->Play(lShake, 1.f, 2);
    CHECK(lId != 0);
    CheckShaken(lWorld.Draw(0.5f), {}, 1.f);

    lWorld.Tick(20);
    const double lStop = lWorld.TheWorld->GetSteppedTime();
    lWorld.Shakes->Stop(lId);

    // Frames before the next step still shake at full strength; then it fades over its BlendOut.
    CheckShaken(lWorld.Draw(0.5f), lShake->Sample(2, lWorld.GameTime(0.5f) - lStart, {}), 1.f);
    lWorld.Tick(6);
    CheckShaken(lWorld.Draw(0.5f), lShake->Sample(2, lWorld.GameTime(0.5f) - lStart, lStop - lStart), 1.f);
    CHECK(lShake->Envelope(lWorld.GameTime(0.5f) - lStart, lStop - lStart) < 1.f);

    lWorld.Tick(20);
    CHECK(lWorld.Shakes->GetPlaying().empty());
    CHECK(lWorld.Shakes->Play(TAssetRef<CameraShake>{}) == 0);
}

TEST_CASE("CameraShakeSubsystem: a source shakes less the farther the view is, and stops with its entity")
{
    ShakeWorld  lWorld;
    CameraShake lHit;
    lHit.Duration = 0.f;
    lHit.BlendIn  = 0.f;
    lHit.BlendOut = 0.25f;
    lWorld.Save("Hit.opaaxasset", lHit);

    Entity lSource = lWorld.MakeSource("Hit.opaaxasset", { 300.f, 0.f });
    lSource.Get<CameraShakeSourceComponent>().InnerRadius = 100.f;
    lSource.Get<CameraShakeSourceComponent>().OuterRadius = 500.f;
    lSource.Get<CameraShakeSourceComponent>().Seed        = 3;

    // Seen in the first step, and played from its start.
    lWorld.Tick(1);
    REQUIRE(lWorld.Shakes->GetPlaying().size() == 1);
    CHECK(lWorld.Shakes->GetPlaying()[0].Start == 0.0);
    CHECK(lWorld.Shakes->GetPlaying()[0].Source == lSource.GetHandle());

    // 300 from the view's centre: half its strength.
    lWorld.Tick(5);
    CheckShaken(lWorld.Draw(0.5f), lHit.Sample(3, lWorld.GameTime(0.5f), {}), 0.5f);

    // Beyond the outer radius from the next step: none.
    lSource.Get<TransformComponent>().Position = { 800.f, 0.f };
    lWorld.Tick(1);
    CheckShaken(lWorld.Draw(0.5f), {}, 1.f);

    // Gone: stopped in that step, faded out, then let go.
    lSource.Get<TransformComponent>().Position = { 0.f, 0.f };
    lWorld.Tick(1);
    lWorld.TheWorld->DestroyEntity(lSource);
    lWorld.Tick(1);
    REQUIRE(lWorld.Shakes->GetPlaying().size() == 1);
    CHECK(lWorld.Shakes->GetPlaying()[0].StoppedAt.has_value());
    lWorld.Tick(20);
    CHECK(lWorld.Shakes->GetPlaying().empty());
}

TEST_CASE("CameraShakeSubsystem: a trauma source shakes at its trauma squared, from the step it was hit")
{
    constexpr Uint64 HIT_STEP = 5;
    constexpr double HIT_AT   = HIT_STEP * STEP_SECONDS;

    ShakeWorld  lWorld;
    CameraShake lRumble;
    lRumble.Pattern  = ECameraShakePattern::Wave;
    lRumble.Duration = 0.f;
    lRumble.BlendIn  = 0.f;
    lWorld.Save("Rumble.opaaxasset", lRumble);

    Entity lSource = lWorld.MakeSource("Rumble.opaaxasset", { 0.f, 0.f });
    CameraShakeSourceComponent& lSettings = lSource.Get<CameraShakeSourceComponent>();
    lSettings.bPlayOnStart = false;
    lSettings.bUseTrauma   = true;
    lSettings.TraumaDecay  = 0.5f;
    lSettings.Seed         = 1;

    // Played from the start, still while its trauma is 0.
    lWorld.Tick(1);
    REQUIRE(lWorld.Shakes->GetPlaying().size() == 1);
    CheckShaken(lWorld.Draw(0.5f), {}, 1.f);

    const EntityID lHandle = lSource.GetHandle();
    lWorld.Cue(HIT_STEP, [lHandle](CameraShakeSubsystem& InShakes) { InShakes.AddTrauma(lHandle, 0.8f); });
    lWorld.Tick(10);

    for (const float lAlpha : { 0.f, 0.3f, 0.9f })
    {
        const double lTime   = lWorld.GameTime(lAlpha);
        const float  lTrauma = 0.8f - 0.5f * static_cast<float>(lTime - HIT_AT);
        CheckShaken(lWorld.Draw(lAlpha), lRumble.Sample(1, lTime, {}), lTrauma * lTrauma);
    }
    CHECK(lWorld.Shakes->GetTrauma(lHandle) ==
          doctest::Approx(0.8f - 0.5f * static_cast<float>(lWorld.TheWorld->GetSteppedTime() - HIT_AT)));

    // An entity that is not a trauma source: nothing happens.
    Entity lPlain = lWorld.MakeSource("Rumble.opaaxasset", { 0.f, 0.f });
    lPlain.Get<CameraShakeSourceComponent>().bPlayOnStart = false;
    lWorld.Shakes->AddTrauma(lPlain.GetHandle(), 1.f);
    CHECK(lWorld.Shakes->GetPlaying().size() == 1);
}
