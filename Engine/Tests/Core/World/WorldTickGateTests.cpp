// Suite: WorldManager's PIE tick gate — pause and step (M4 S5).
//
// WHY THIS EXISTS.
//   The editor's Pause/Step must not reach into the frame loop; it sets a flag and WorldManager's
//   own tick path honours it. The subtle part is not "skip the update" — it is that Engine::Loop
//   calls Update ONCE and FixedUpdate 0..N times per frame, so the two hooks have to agree about
//   whether THIS frame is ticking. The decision is therefore taken once, in Update, and FixedUpdate
//   only reads it. A step that advanced Update alone would starve the fixed step and desync a
//   physics world from what the viewport shows.
//
//   Everything below drives WorldManager::Update / FixedUpdate directly — the same calls
//   Engine::Loop makes — rather than the flags in isolation, because the ordering IS the design.
#include <doctest.h>

#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
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
    class CounterSubsystem : public WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(CounterSubsystem)

        explicit CounterSubsystem(WorldContext&) {}

        bool Startup() override  { return true; }
        void Shutdown() override {}

        void Update(double)      override { ++Updates; }
        void FixedUpdate(double) override { ++FixedUpdates; }

        Uint64 Updates      = 0;
        Uint64 FixedUpdates = 0;
    };

    // A bare manager creates worlds with no subsystems (it has no engine to build a context from),
    // so the counter is injected the way CreateSubsystemsFor would have: context first, then
    // register, then StartupAll. std::ref is load-bearing — see WS4.
    struct GatedWorld
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        WorldManager     Worlds;
        World*           TheWorld = nullptr;
        CounterSubsystem* Counter = nullptr;

        explicit GatedWorld(EWorldMode InMode = EWorldMode::Play)
        {
            TheWorld = Worlds.CreateWorld("Gated", InMode);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{*TheWorld, Resources, Events, Debug});
            TheWorld->GetSubsystems().RegisterSubsystem<CounterSubsystem>(std::ref(*TheWorld->GetContext()));
            TheWorld->GetSubsystems().StartupAll();

            Counter = TheWorld->GetSubsystems().GetSubsystem<CounterSubsystem>();
            REQUIRE(Counter != nullptr);

            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        // One frame the way Engine::Loop drives it: Update once, then the fixed steps it owes.
        void TickFrame(Uint64 InFixedSteps = 1)
        {
            Worlds.Update(0.016);
            for (Uint64 lStep = 0; lStep < InFixedSteps; ++lStep)
            {
                Worlds.FixedUpdate(1.0 / 60.0);
            }
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
    CHECK_FALSE(lWorlds.IsTickingThisFrame());

    lWorlds.SetPaused(false);
    lWorlds.Update(0.016);
    CHECK(lWorlds.IsTickingThisFrame());   // decided, even with nothing to tick
}
