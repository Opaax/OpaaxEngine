// Suite: the engine lifetime events (EngineStarted / EngineTearingDown) on a bare EventBus
// (starting a real Engine needs a GPU context). Two empty structs must stay distinct on the bus.
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
