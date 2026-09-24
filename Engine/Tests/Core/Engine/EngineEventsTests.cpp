// Suite: the engine's own Tier-3 lifetime payloads (EngineStarted / EngineTearingDown).
//
// These test the PAYLOADS against a bare EventBus, not the publish sites in Engine::Startup
// and Engine::TearDown: starting a real Engine boots every subsystem, RendererManager
// included, which needs a GPU context a headless runner has no way to provide. The publish
// sites are covered by running a host (see the milestone notes).
//
// The property worth pinning is that two EMPTY structs stay distinct on the bus — the bus
// keys by a hash of the type name, and empty types are exactly where a keying mistake would
// silently deliver one lifetime event as the other.
#include <doctest.h>

#include "Core/Events/EventBus.h"
#include "Engine/EngineEvents.h"

using namespace Opaax;

TEST_CASE("EngineEvents: the two lifetime payloads dispatch independently")
{
    EventBus lBus;

    int lStarted = 0;
    int lTearing = 0;

    lBus.Subscribe<EngineStarted>([&lStarted](const EngineStarted&) { ++lStarted; });
    lBus.Subscribe<EngineTearingDown>([&lTearing](const EngineTearingDown&) { ++lTearing; });

    lBus.Publish(EngineStarted{});
    CHECK(lStarted == 1);
    CHECK(lTearing == 0);   // an empty struct must not answer to its empty sibling

    lBus.Publish(EngineTearingDown{});
    CHECK(lStarted == 1);
    CHECK(lTearing == 1);
}

TEST_CASE("EngineEvents: Unsubscribe stops delivery")
{
    EventBus lBus;

    int lCount = 0;
    const DelegateHandle lHandle =
        lBus.Subscribe<EngineTearingDown>([&lCount](const EngineTearingDown&) { ++lCount; });

    lBus.Publish(EngineTearingDown{});
    REQUIRE(lCount == 1);

    CHECK(lBus.Unsubscribe(lHandle));
    lBus.Publish(EngineTearingDown{});
    CHECK(lCount == 1);
}
