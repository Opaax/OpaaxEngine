// Suite: WorldSubsystemRegistry — candidates, filtering, and ctor injection (M4 S3).
//
// WHY THIS EXISTS.
//   A world runs a FILTERED set of subsystems chosen at creation. The registry holds
//   CANDIDATES; each world walks them and takes the ones whose static ShouldCreate accepts it.
//   That is the mechanism behind M4's headline gate — "an Edit-only overlay never EXISTS in a
//   Play world" — and the reason ShouldCreate is static: deciding needs no instance, so a
//   rejected candidate is never constructed rather than constructed-then-ignored.
//
//   The subsystems below take a WorldContext& by constructor. That is not decoration: it is
//   the fix for Editor.md §3's stated injection rule, which cannot work — the registration site
//   `WorldSubsystems().Register<T>()` takes no arguments (frozen by MR1), so there is nowhere
//   to capture a dependency, and without the context a subsystem would have to reach the
//   AppServiceLocator, which D3 forbids.
//
// WHAT THIS DOES NOT COVER.
//   The tick path and the real per-world creation inside WorldManager need a started engine
//   (WorldManager::Startup resolves ResourceManager/EngineEventBus/DebugDraw). A bare manager
//   in a test has no engine, so it creates worlds with NO subsystems — asserted below as the
//   documented behaviour, not worked around. The wiring gate is the hosts' ordered boot log
//   (L22), exactly as it was for S2.
#include <doctest.h>

#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Renderer/DebugDraw.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/Systems/WorldSubsystemRegistry.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    OpaaxStringID Name(const char* InText) { return OpaaxStringID(OpaaxString(InText)); }

    // -------------------------------------------------------------------------
    // Always created — the common case, which must cost NO boilerplate. Note the
    // absence of a ShouldCreate: the entry detects that and defaults to "create".
    // -------------------------------------------------------------------------
    class AlwaysSubsystem : public WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(AlwaysSubsystem)

        explicit AlwaysSubsystem(WorldContext& InContext) : m_Context(&InContext) {}

        bool Startup() override  { m_bStarted = true; return true; }
        void Shutdown() override { m_bStarted = false; }

        void Update(double InDelta) override { m_Accumulated += InDelta; }

        bool          IsStarted()   const noexcept { return m_bStarted; }
        double        Accumulated() const noexcept { return m_Accumulated; }
        WorldContext* Context()     const noexcept { return m_Context; }

    private:
        WorldContext* m_Context     = nullptr;
        bool          m_bStarted    = false;
        double        m_Accumulated = 0.0;
    };

    // -------------------------------------------------------------------------
    // Edit-only — the S5 shape (an editor overlay). One static function is the whole
    // cost of being selective.
    // -------------------------------------------------------------------------
    class EditOnlySubsystem : public WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(EditOnlySubsystem)

        explicit EditOnlySubsystem(WorldContext&) {}

        static bool ShouldCreate(const World& InWorld) { return InWorld.GetMode() == EWorldMode::Edit; }

        bool Startup() override  { return true; }
        void Shutdown() override {}
    };

    class PlayOnlySubsystem : public WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(PlayOnlySubsystem)

        explicit PlayOnlySubsystem(WorldContext&) {}

        static bool ShouldCreate(const World& InWorld) { return InWorld.GetMode() == EWorldMode::Play; }

        bool Startup() override  { return true; }
        void Shutdown() override {}
    };
}

// =============================================================================
// Registration
// =============================================================================
TEST_CASE("world subsystem registry: registers candidates and finds them by name")
{
    WorldSubsystemRegistry lRegistry;

    REQUIRE(lRegistry.Register<AlwaysSubsystem>(Name("Always")));
    REQUIRE(lRegistry.Register<EditOnlySubsystem>(Name("EditOnly")));

    CHECK(lRegistry.Count() == 2u);
    CHECK(lRegistry.FindByName(Name("Always")) != nullptr);
    CHECK(lRegistry.FindByName(Name("EditOnly")) != nullptr);
    CHECK(lRegistry.FindByName(Name("Nope")) == nullptr);
}

TEST_CASE("world subsystem registry: refuses a duplicate name and an empty one")
{
    WorldSubsystemRegistry lRegistry;

    REQUIRE(lRegistry.Register<AlwaysSubsystem>(Name("Taken")));

    CHECK_FALSE(lRegistry.Register<EditOnlySubsystem>(Name("Taken")));
    CHECK_FALSE(lRegistry.Register<PlayOnlySubsystem>(OpaaxStringID{}));

    CHECK(lRegistry.Count() == 1u);
}

TEST_CASE("world subsystem registry: sealing refuses later registration")
{
    WorldSubsystemRegistry lRegistry;

    REQUIRE(lRegistry.Register<AlwaysSubsystem>(Name("Always")));

    lRegistry.Seal();
    CHECK(lRegistry.IsSealed());

    // A candidate accepted now would simply be absent from the world that already exists.
    CHECK_FALSE(lRegistry.Register<EditOnlySubsystem>(Name("EditOnly")));
    CHECK(lRegistry.Count() == 1u);

    lRegistry.Seal(); // idempotent (LC3)
    CHECK(lRegistry.IsSealed());
}

TEST_CASE("world subsystem registry: EngineRegistries seals BOTH registries together")
{
    EngineRegistries lRegistries;

    REQUIRE(lRegistries.WorldSubsystems().Register<AlwaysSubsystem>(Name("Always")));

    lRegistries.SealAll();

    // MR0's payoff: one seal covers every registry, so a second one cannot be forgotten.
    CHECK(lRegistries.Components().IsSealed());
    CHECK(lRegistries.WorldSubsystems().IsSealed());
}

// =============================================================================
// Filtering — the headline behaviour
// =============================================================================
TEST_CASE("world subsystem registry: ShouldCreate selects by world mode")
{
    WorldSubsystemRegistry lRegistry;

    REQUIRE(lRegistry.Register<EditOnlySubsystem>(Name("EditOnly")));
    REQUIRE(lRegistry.Register<PlayOnlySubsystem>(Name("PlayOnly")));
    REQUIRE(lRegistry.Register<AlwaysSubsystem>(Name("Always")));

    World lEditWorld("E", EWorldMode::Edit);
    World lPlayWorld("P", EWorldMode::Play);

    const IWorldSubsystemEntry* lEditOnly = lRegistry.FindByName(Name("EditOnly"));
    const IWorldSubsystemEntry* lPlayOnly = lRegistry.FindByName(Name("PlayOnly"));
    const IWorldSubsystemEntry* lAlways   = lRegistry.FindByName(Name("Always"));

    REQUIRE(lEditOnly != nullptr);
    REQUIRE(lPlayOnly != nullptr);
    REQUIRE(lAlways != nullptr);

    CHECK(lEditOnly->ShouldCreate(lEditWorld));
    CHECK_FALSE(lEditOnly->ShouldCreate(lPlayWorld));

    CHECK_FALSE(lPlayOnly->ShouldCreate(lEditWorld));
    CHECK(lPlayOnly->ShouldCreate(lPlayWorld));

    // No ShouldCreate declared => created everywhere, with no boilerplate on the type.
    CHECK(lAlways->ShouldCreate(lEditWorld));
    CHECK(lAlways->ShouldCreate(lPlayWorld));
}

// =============================================================================
// Creation into a world — ctor injection, ordering, lifecycle
// =============================================================================
TEST_CASE("world subsystem registry: CreateInto constructs from the context and StartupAll starts it")
{
    WorldSubsystemRegistry lRegistry;
    REQUIRE(lRegistry.Register<AlwaysSubsystem>(Name("Always")));

    World lWorld("Subject", EWorldMode::Play);

    // Stand in for what WorldManager::CreateWorld composes. These are the REAL types, just never
    // started — each is default-constructible and other suites already stack-allocate them, so
    // the context carries valid references rather than anything cast into place.
    ResourceManager lResources;
    EngineEventBus  lEvents;
    DebugDraw       lDebug;

    WorldContext lContext{lWorld, lResources, lEvents, lDebug};

    const IWorldSubsystemEntry* lEntry = lRegistry.FindByName(Name("Always"));
    REQUIRE(lEntry != nullptr);

    lWorld.SetContext(lContext);
    REQUIRE(lWorld.GetContext() != nullptr);

    lEntry->CreateInto(lWorld.GetSubsystems(), *lWorld.GetContext());
    lWorld.GetSubsystems().StartupAll(); // consumes AND CLEARS the factory list

    AlwaysSubsystem* lSubsystem = lWorld.GetSubsystems().GetSubsystem<AlwaysSubsystem>();
    REQUIRE(lSubsystem != nullptr);

    CHECK(lSubsystem->IsStarted());

    // THE discriminating assertion. RegisterSubsystem captures ctor args BY VALUE, so a context
    // passed by reference would have been copied into the factory lambda that StartupAll just
    // cleared — and the subsystem would hold a pointer into freed memory. Comparing the stored
    // address against the WORLD'S context proves it points at the long-lived one: a lambda-local
    // copy would have a different address. Reading OwningWorld instead would NOT discriminate,
    // since freed memory usually still holds the old value.
    REQUIRE(lSubsystem->Context() != nullptr);
    CHECK(lSubsystem->Context() == lWorld.GetContext());
    CHECK(&lSubsystem->Context()->OwningWorld == &lWorld);

    // The tick path WorldManager::Update drives.
    lWorld.GetSubsystems().UpdateAll(0.5);
    CHECK(lSubsystem->Accumulated() == doctest::Approx(0.5));

    // Idempotent shutdown (LC3): DestroyWorld calls it, then ~World repeats it.
    lWorld.ShutdownSubsystems();
    CHECK_FALSE(lSubsystem->IsStarted());
    lWorld.ShutdownSubsystems();
}

// =============================================================================
// The bare-manager case, asserted rather than worked around
// =============================================================================
TEST_CASE("world subsystem registry: a manager with no engine creates a world with no subsystems")
{
    // WorldManager::Startup resolves its engine siblings; a bare manager never started, so
    // there is no context to construct subsystems from. A world with an empty subsystem list is
    // the correct outcome — and it is why every other test here drives the registry directly.
    WorldManager lWorlds;

    World* lWorld = lWorlds.CreateWorld("Bare", EWorldMode::Play);

    REQUIRE(lWorld != nullptr);
    CHECK(lWorld->GetSubsystems().GetSystems().empty());
    CHECK(lWorld->GetContext() == nullptr);
}
