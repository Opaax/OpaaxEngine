// Suite: MoverComponent mode-set surface (the pure, component-level rules).
//
// MoverComponent is OPAAX_API (reached via the import lib). Only the data-level
// mode-set logic is unit-tested here — SupportsMode / AddMode / RemoveMode (the Modes
// array) + QueueNextMode (sets the pending fields) + GetCurrentMode. The self-heal and
// QueueNextMode-rejection rules live in PhysicsSubsystem's mover tick (need a physics world) and are
// deferred to a future integration test. AddMode's param-minting goes through the global
// MoverModeRegistry, which is empty in a test process — but AddMode still records the
// mode in Modes regardless, so the mode-set behaviour is testable standalone.
#include <doctest.h>

#include "ECS/Components/MoverComponent.h"
#include "Core/OpaaxStringID.hpp"

using namespace Opaax;
using Opaax::ECS::MoverComponent;

TEST_CASE("MoverComponent: defaults to the GroundMove mode only")
{
    MoverComponent lMover;
    CHECK(lMover.Modes.size() == 1u);
    CHECK(lMover.SupportsMode(OPAAX_ID("GroundMove")));
    CHECK_FALSE(lMover.SupportsMode(OPAAX_ID("Fly")));
    CHECK(lMover.GetCurrentMode() == OPAAX_ID("GroundMove"));
}

TEST_CASE("MoverComponent: AddMode extends the supported set; duplicates are no-ops")
{
    MoverComponent lMover;
    lMover.AddMode(OPAAX_ID("Fly"));
    CHECK(lMover.SupportsMode(OPAAX_ID("Fly")));
    CHECK(lMover.Modes.size() == 2u);

    lMover.AddMode(OPAAX_ID("GroundMove")); // already present
    CHECK(lMover.Modes.size() == 2u);
}

TEST_CASE("MoverComponent: RemoveMode drops the mode from the supported set")
{
    MoverComponent lMover;
    lMover.AddMode(OPAAX_ID("Fly"));
    lMover.RemoveMode(OPAAX_ID("GroundMove"));

    CHECK_FALSE(lMover.SupportsMode(OPAAX_ID("GroundMove")));
    CHECK(lMover.SupportsMode(OPAAX_ID("Fly")));
    CHECK(lMover.Modes.size() == 1u);
}

TEST_CASE("MoverComponent: QueueNextMode stages the switch without applying it")
{
    MoverComponent lMover;
    lMover.AddMode(OPAAX_ID("Fly"));

    lMover.QueueNextMode(OPAAX_ID("Fly"));
    // The active mode is unchanged until the subsystem applies the queued switch.
    CHECK(lMover.GetCurrentMode() == OPAAX_ID("GroundMove"));
    CHECK(lMover.PendingMode == OPAAX_ID("Fly"));
    CHECK_FALSE(lMover.PendingReenter);

    lMover.QueueNextMode(OPAAX_ID("Fly"), /*bReenter=*/true);
    CHECK(lMover.PendingReenter);
}
