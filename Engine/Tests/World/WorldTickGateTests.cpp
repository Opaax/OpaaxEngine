// Suite: WorldManager's tick gate — pause and step. Engine::Loop calls Update once and
// FixedUpdate 0..N times, so the decision is taken once in Update and FixedUpdate reads it
// (a step must advance both). Driven through Update / FixedUpdate like Engine::Loop.
#include <doctest.h>

#include "Application/Services/IPaths.h"   // IPaths::Null() — the context's paths reference
#include "Core/Profiling/FrameProfiler.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Config/EngineConfigData.h"   // the context's config reference
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Resources/ResourceManager.h"
#include "Renderer/DebugDraw.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    // Counts what actually reached the world, so "did not tick" is a measurement rather than the
    // absence of a log line.
    // Its Present queues one line, as a debug subsystem does.
    class CounterSubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(CounterSubsystem, WorldSubsystem)

    public:
        void Update(double InDeltaTime) override
        {
            ++Updates;
            LastUpdateDelta = InDeltaTime;
        }

        void FixedUpdate(double) override { ++FixedUpdates; }

        void Present(double InRealDeltaTime) override
        {
            ++Presents;
            LastPresentDelta      = InRealDeltaTime;
            FixedUpdatesAtPresent = FixedUpdates;
            GetContext().Debug.DrawLine({ 0.f, 0.f }, { 10.f, 0.f }, { 1.f, 1.f, 1.f, 1.f });
        }

        Uint64 Updates               = 0;
        Uint64 FixedUpdates          = 0;
        Uint64 Presents              = 0;
        Uint64 FixedUpdatesAtPresent = 0;
        double LastUpdateDelta       = 0.0;
        double LastPresentDelta      = 0.0;
    };

    // A bare manager creates worlds with no subsystems (it has no engine to build a context from),
    // so the counter is added the way CreateSubsystemsFor would have: context first, then the class,
    // then StartupAll.
    struct GatedWorld
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug{ true };   // queues in any build
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;
        World*           TheWorld = nullptr;
        CounterSubsystem* Counter = nullptr;

        // Lines queued by the last frame, read before the frame's clear.
        Uint64 LinesDrawn = 0;

        explicit GatedWorld(EWorldMode InMode = EWorldMode::Play)
        {
            TheWorld = Worlds.CreateWorld("Gated", InMode);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{*TheWorld, Resources, IPaths::Null(), Events, Input, Config, /*Actions*/ nullptr, /*UI*/ nullptr, Debug});
            TheWorld->GetSubsystems().Add<CounterSubsystem>();
            TheWorld->GetSubsystems().StartupAll();

            Counter = TheWorld->GetSubsystems().GetSubsystem<CounterSubsystem>();
            REQUIRE(Counter != nullptr);

            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        // One frame the way Engine::Loop drives it: Update once, then the fixed steps it owes, then
        // Render; the renderer draws the debug queue, then clears it.
        void TickFrame(Uint64 InFixedSteps = 1)
        {
            Worlds.Update(0.016);
            for (Uint64 lStep = 0; lStep < InFixedSteps; ++lStep)
            {
                Worlds.FixedUpdate(1.0 / 60.0);
            }
            Worlds.Render(1.0);

            LinesDrawn = static_cast<Uint64>(Debug.GetLines().size());
            Debug.Clear();
        }
    };
}

// =============================================================================
// The default: nothing is gated until something pauses
// =============================================================================
TEST_CASE("world tick gate: an unpaused manager ticks the active world every frame")
{
    GatedWorld lFixture;

    CHECK_FALSE(lFixture.Worlds.IsPaused());

    lFixture.TickFrame(2);
    lFixture.TickFrame(2);

    CHECK(lFixture.Counter->Updates == 2u);
    CHECK(lFixture.Counter->FixedUpdates == 4u);
    CHECK(lFixture.Worlds.IsTickingThisFrame());
}

// =============================================================================
// Pause
// =============================================================================
TEST_CASE("world tick gate: pausing stops BOTH Update and FixedUpdate")
{
    GatedWorld lFixture;

    lFixture.TickFrame(2);
    REQUIRE(lFixture.Counter->Updates == 1u);
    REQUIRE(lFixture.Counter->FixedUpdates == 2u);

    lFixture.Worlds.SetPaused(true);

    lFixture.TickFrame(2);
    lFixture.TickFrame(2);

    // A paused world is frozen, not slowed: the fixed steps must not leak through, or a paused
    // physics world would keep integrating while the Update-driven systems stood still.
    CHECK(lFixture.Counter->Updates == 1u);
    CHECK(lFixture.Counter->FixedUpdates == 2u);
    CHECK_FALSE(lFixture.Worlds.IsTickingThisFrame());

    lFixture.Worlds.SetPaused(false);
    lFixture.TickFrame(2);

    CHECK(lFixture.Counter->Updates == 2u);
    CHECK(lFixture.Counter->FixedUpdates == 4u);
}

// =============================================================================
// Step — the reason the decision is per-frame
// =============================================================================
TEST_CASE("world tick gate: a step advances exactly ONE frame, fixed steps included")
{
    GatedWorld lFixture;

    lFixture.Worlds.SetPaused(true);
    lFixture.TickFrame(3);
    REQUIRE(lFixture.Counter->Updates == 0u);

    lFixture.Worlds.RequestStep();
    lFixture.TickFrame(3);

    // A step is a FRAME, not an Update: the 3 fixed steps this frame owed ran too.
    CHECK(lFixture.Counter->Updates == 1u);
    CHECK(lFixture.Counter->FixedUpdates == 3u);

    // ...and then it is paused again, with no lingering permission to tick.
    lFixture.TickFrame(3);
    CHECK(lFixture.Counter->Updates == 1u);
    CHECK(lFixture.Counter->FixedUpdates == 3u);
    CHECK(lFixture.Worlds.IsPaused());
}

TEST_CASE("world tick gate: FixedUpdate follows the decision Update took THAT frame")
{
    GatedWorld lFixture;

    // Pause arriving mid-frame (a click lands between the two hooks) must not split the frame in
    // half — Update already ran, so its fixed steps still run. The pause takes effect next frame.
    lFixture.Worlds.Update(0.016);
    lFixture.Worlds.SetPaused(true);
    lFixture.Worlds.FixedUpdate(1.0 / 60.0);

    CHECK(lFixture.Counter->Updates == 1u);
    CHECK(lFixture.Counter->FixedUpdates == 1u);

    lFixture.TickFrame(1);
    CHECK(lFixture.Counter->Updates == 1u);
    CHECK(lFixture.Counter->FixedUpdates == 1u);
}

TEST_CASE("world tick gate: a step requested while UNPAUSED changes nothing")
{
    GatedWorld lFixture;

    lFixture.Worlds.RequestStep();   // the editor refuses this, but the gate must not double-tick
    lFixture.TickFrame(1);

    CHECK(lFixture.Counter->Updates == 1u);
    CHECK(lFixture.Counter->FixedUpdates == 1u);

    // The request was consumed, not banked: pausing now really pauses.
    lFixture.Worlds.SetPaused(true);
    lFixture.TickFrame(1);

    CHECK(lFixture.Counter->Updates == 1u);
}

// =============================================================================
// Scope of the gate
// =============================================================================
TEST_CASE("world tick gate: the gate is about the TICK, not about the world's mode")
{
    // An Edit world pauses exactly like a Play one. Mode-blind on purpose: a gate that inspected
    // the mode would need a reason no caller has, and PIE is the only thing that pauses anyway.
    GatedWorld lFixture(EWorldMode::Edit);

    lFixture.Worlds.SetPaused(true);
    lFixture.TickFrame(1);

    CHECK(lFixture.Counter->Updates == 0u);
}

TEST_CASE("world tick gate: ticking with no active world is safe and still decides")
{
    WorldManager lWorlds;

    lWorlds.SetPaused(true);
    lWorlds.Update(0.016);
    lWorlds.FixedUpdate(1.0 / 60.0);
    lWorlds.Render(1.0);
    CHECK_FALSE(lWorlds.IsTickingThisFrame());

    lWorlds.SetPaused(false);
    lWorlds.Update(0.016);
    CHECK(lWorlds.IsTickingThisFrame());   // decided, even with nothing to tick
}

// =============================================================================
// Present — every frame, paused or not
// =============================================================================
TEST_CASE("world tick gate: a paused world still presents, so its debug line is drawn frame after frame")
{
    GatedWorld lFixture;

    lFixture.TickFrame(2);
    REQUIRE(lFixture.LinesDrawn == 1u);

    lFixture.Worlds.SetPaused(true);

    for (Uint64 lFrame = 1; lFrame <= 3; ++lFrame)
    {
        lFixture.TickFrame(2);

        // Queued again this frame: the renderer cleared the last one.
        CHECK(lFixture.LinesDrawn == 1u);
        CHECK(lFixture.Counter->Presents == 1u + lFrame);
    }

    // Presenting moved nothing: the world itself stayed frozen.
    CHECK(lFixture.Counter->Updates == 1u);
    CHECK(lFixture.Counter->FixedUpdates == 2u);
}

TEST_CASE("world tick gate: Present gets the frame's real delta, whatever the time scale")
{
    GatedWorld lFixture;

    lFixture.Worlds.SetTimeScale(0.25f);
    lFixture.TickFrame(1);

    CHECK(lFixture.Counter->LastUpdateDelta == doctest::Approx(0.004));
    CHECK(lFixture.Counter->LastPresentDelta == doctest::Approx(0.016));

    lFixture.Worlds.SetPaused(true);
    lFixture.Counter->LastPresentDelta = 0.0;
    lFixture.TickFrame(1);

    CHECK(lFixture.Counter->LastPresentDelta == doctest::Approx(0.016));
}

TEST_CASE("world tick gate: a stepped frame presents once, after its fixed steps")
{
    GatedWorld lFixture;

    lFixture.Worlds.SetPaused(true);
    lFixture.TickFrame(3);
    REQUIRE(lFixture.Counter->Presents == 1u);

    lFixture.Worlds.RequestStep();
    lFixture.TickFrame(3);

    CHECK(lFixture.Counter->Presents == 2u);
    CHECK(lFixture.Counter->FixedUpdates == 3u);
    CHECK(lFixture.Counter->FixedUpdatesAtPresent == 3u);   // the step's state, not the one before
    CHECK(lFixture.LinesDrawn == 1u);
}
