// Suite: WorldManager Tier-2 lifetime delegates (OnWorldCreated / OnWorldDestroyed /
// OnActiveWorldChanged).
//
// WorldManager depends on nothing to broadcast — no bus, no service locator, no Engine —
// so these construct a bare manager and bind a lambda. That independence is the point of
// the design: Engine binds these same delegates to bridge them onto the EngineEventBus
// (see Engine::HandleWorldCreated), but nothing here needs the Engine to exist.
//
// Every case builds the manager WITHOUT calling Startup(), because Startup() creates and
// activates a default "Main" world whose events would otherwise pollute the counts.
#include <doctest.h>

#include "World/World.h"
#include "World/WorldEvents.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    // Member-bound listener — exercises AddMember/RemoveAll, the exact path Engine uses
    // to bind its bus bridge in Startup and drop it in Shutdown.
    class CreatedListener
    {
    public:
        void OnCreated(World* InWorld)
        {
            ++Count;
            Last = InWorld;
        }

        int    Count = 0;
        World* Last  = nullptr;
    };
}

// =============================================================================
// Create
// =============================================================================
TEST_CASE("WorldManager: CreateWorld broadcasts OnWorldCreated with the world it returns")
{
    WorldManager lManager;

    int    lCount = 0;
    World* lSeen  = nullptr;

    lManager.OnWorldCreated.Add([&](World* InWorld)
    {
        ++lCount;
        lSeen = InWorld;
    });

    World* lWorld = lManager.CreateWorld("Test");

    CHECK(lCount == 1);
    CHECK(lSeen == lWorld);
    CHECK(lManager.GetWorldCount() == 1u);
}

// =============================================================================
// Activation
// =============================================================================
TEST_CASE("WorldManager: SetActiveWorld reports (old, new), with a null old on first activation")
{
    WorldManager lManager;

    World* lA = lManager.CreateWorld("A");
    World* lB = lManager.CreateWorld("B");

    World* lOld   = lB;   // seeded non-null so a missing broadcast can't false-pass
    World* lNew   = nullptr;
    int    lCount = 0;

    lManager.OnActiveWorldChanged.Add([&](World* InOld, World* InNew)
    {
        ++lCount;
        lOld = InOld;
        lNew = InNew;
    });

    REQUIRE(lManager.SetActiveWorld(lA));
    CHECK(lCount == 1);
    CHECK(lOld == nullptr); // nothing was active before
    CHECK(lNew == lA);

    REQUIRE(lManager.SetActiveWorld(lB));
    CHECK(lCount == 2);
    CHECK(lOld == lA);
    CHECK(lNew == lB);
}

TEST_CASE("WorldManager: re-activating the already-active world is a no-op and broadcasts nothing")
{
    WorldManager lManager;

    World* lWorld = lManager.CreateWorld("Only");
    REQUIRE(lManager.SetActiveWorld(lWorld));

    int lCount = 0;
    lManager.OnActiveWorldChanged.Add([&](World*, World*) { ++lCount; });

    // Still reports success — it IS active — but must not deactivate/reactivate in place
    // nor emit a meaningless ActiveWorldChanged(X, X).
    CHECK(lManager.SetActiveWorld(lWorld));
    CHECK(lCount == 0);
    CHECK(lManager.GetActiveWorld() == lWorld);
}

TEST_CASE("WorldManager: SetActiveWorld(nullptr) is rejected and broadcasts nothing")
{
    WorldManager lManager;

    World* lWorld = lManager.CreateWorld("Only");
    REQUIRE(lManager.SetActiveWorld(lWorld));

    int lCount = 0;
    lManager.OnActiveWorldChanged.Add([&](World*, World*) { ++lCount; });

    CHECK_FALSE(lManager.SetActiveWorld(nullptr));
    CHECK(lCount == 0);
    CHECK(lManager.GetActiveWorld() == lWorld); // the active world is untouched
}

// =============================================================================
// Destroy
// =============================================================================
TEST_CASE("WorldManager: destroying the active world deactivates it first, then destroys it")
{
    WorldManager lManager;

    World* lWorld = lManager.CreateWorld("Doomed");
    REQUIRE(lManager.SetActiveWorld(lWorld));

    TDynArray<OpaaxString> lOrder;
    OpaaxString            lNameSeenWhileDying;

    lManager.OnActiveWorldChanged.Add([&](World* InOld, World* InNew)
    {
        lOrder.push_back(OpaaxString("deactivated"));
        CHECK(InOld == lWorld);
        CHECK(InNew == nullptr); // the active slot is empty, not reassigned
    });

    lManager.OnWorldDestroyed.Add([&](World* InWorld)
    {
        lOrder.push_back(OpaaxString("destroyed"));

        // The guard that matters: WorldManager broadcasts BEFORE erasing, so the world is
        // still alive here. This is the window a listener uses to drop per-world state.
        //
        // Checking the count is what actually pins the ordering: if the broadcast ever
        // moved after the erase, the world would already be gone and this would read 0.
        // The GetName() below would merely be undefined behaviour, which can pass by luck.
        CHECK(lManager.GetWorldCount() == 1u);

        REQUIRE(InWorld != nullptr);
        lNameSeenWhileDying = InWorld->GetName();
    });

    lManager.DestroyWorld(lWorld);

    REQUIRE(lOrder.size() == 2u);
    CHECK(lOrder[0] == "deactivated");
    CHECK(lOrder[1] == "destroyed");
    CHECK(lNameSeenWhileDying == "Doomed");

    CHECK(lManager.GetActiveWorld() == nullptr);
    CHECK(lManager.GetWorldCount() == 0u);
}

TEST_CASE("WorldManager: destroying a non-active world leaves the active one alone")
{
    WorldManager lManager;

    World* lActive = lManager.CreateWorld("Active");
    World* lOther  = lManager.CreateWorld("Other");
    REQUIRE(lManager.SetActiveWorld(lActive));

    int lActiveChanges = 0;
    lManager.OnActiveWorldChanged.Add([&](World*, World*) { ++lActiveChanges; });

    World* lDestroyed = nullptr;
    lManager.OnWorldDestroyed.Add([&](World* InWorld) { lDestroyed = InWorld; });

    lManager.DestroyWorld(lOther);

    CHECK(lDestroyed == lOther);
    CHECK(lActiveChanges == 0); // the active world never changed
    CHECK(lManager.GetActiveWorld() == lActive);
    CHECK(lManager.GetWorldCount() == 1u);
}

TEST_CASE("WorldManager: destroying a null world is rejected and broadcasts nothing")
{
    WorldManager lManager;

    lManager.CreateWorld("Keep");

    int lCount = 0;
    lManager.OnWorldDestroyed.Add([&](World*) { ++lCount; });

    lManager.DestroyWorld(nullptr);

    CHECK(lCount == 0);
    CHECK(lManager.GetWorldCount() == 1u);
}

// =============================================================================
// TearDown — phase 1 of the two-phase stop
//
// The whole reason TearDown exists: at Shutdown, Engine has already unbound and the bus is
// on its way out, so a world dying there announces itself to nobody. TearDown runs while
// everything is still alive, so it MUST be the phase that reports dying worlds.
// =============================================================================
TEST_CASE("WorldManager: TearDown announces every remaining world instead of dropping them")
{
    WorldManager lManager;

    World* lA = lManager.CreateWorld("A");
    World* lB = lManager.CreateWorld("B");
    REQUIRE(lManager.SetActiveWorld(lA));

    TDynArray<World*> lDestroyed;
    int               lDeactivations = 0;

    lManager.OnWorldDestroyed.Add([&](World* InWorld) { lDestroyed.push_back(InWorld); });
    lManager.OnActiveWorldChanged.Add([&](World*, World* InNew)
    {
        if (InNew == nullptr) { ++lDeactivations; }
    });

    lManager.TearDown();

    // Both worlds announced — the regression this whole phase exists to prevent.
    REQUIRE(lDestroyed.size() == 2u);
    CHECK(lDeactivations == 1); // only the active one reports a deactivation

    const bool lSawA = (lDestroyed[0] == lA) || (lDestroyed[1] == lA);
    const bool lSawB = (lDestroyed[0] == lB) || (lDestroyed[1] == lB);
    CHECK(lSawA);
    CHECK(lSawB);

    CHECK(lManager.GetWorldCount() == 0u);
    CHECK(lManager.GetActiveWorld() == nullptr);
}

TEST_CASE("WorldManager: Shutdown after TearDown is a safe no-op")
{
    WorldManager lManager;

    lManager.CreateWorld("A");
    lManager.TearDown();

    int lCount = 0;
    lManager.OnWorldDestroyed.Add([&](World*) { ++lCount; });

    lManager.Shutdown(); // must not double-announce, must not crash

    CHECK(lCount == 0);
    CHECK(lManager.GetWorldCount() == 0u);
}

TEST_CASE("WorldManager: TearDown is idempotent")
{
    WorldManager lManager;

    lManager.CreateWorld("A");

    int lCount = 0;
    lManager.OnWorldDestroyed.Add([&](World*) { ++lCount; });

    lManager.TearDown();
    lManager.TearDown(); // nothing left — must not re-announce

    CHECK(lCount == 1);
    CHECK(lManager.GetWorldCount() == 0u);
}

// =============================================================================
// Binding lifetime — the Engine::Shutdown path
// =============================================================================
TEST_CASE("WorldManager: RemoveAll(owner) stops delivery to a member-bound listener")
{
    WorldManager    lManager;
    CreatedListener lListener;

    lManager.OnWorldCreated.AddMember(&lListener, &CreatedListener::OnCreated);

    World* lFirst = lManager.CreateWorld("First");
    CHECK(lListener.Count == 1);
    CHECK(lListener.Last == lFirst);

    // What Engine::Shutdown does — unbind by owner, no handles stored.
    lManager.OnWorldCreated.RemoveAll(&lListener);

    lManager.CreateWorld("Second");
    CHECK(lListener.Count == 1); // unchanged: the binding is gone
    CHECK(lListener.Last == lFirst);
}
