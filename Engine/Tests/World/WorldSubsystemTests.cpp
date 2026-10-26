// Suite: world subsystems as Object classes — found by class (tick order, then name), filtered by
// ShouldCreate on the class default, given the world's context before Startup, and made all before
// any starts. A bare manager (no engine) creates worlds with no subsystems; that is asserted as is.
#include <doctest.h>

#include "Application/Services/IPaths.h"   // IPaths::Null() — the context's paths reference
#include "Engine/Config/EngineConfigData.h"   // the context's config reference
#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    // -------------------------------------------------------------------------
    // Always created — the common case, which costs no boilerplate: no ShouldCreate, no order.
    // -------------------------------------------------------------------------
    class AlwaysSubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(AlwaysSubsystem, WorldSubsystem)

    public:
        bool Startup() override
        {
            m_Context  = &GetContext();
            m_bStarted = true;
            ++s_StartCount;
            return true;
        }

        void Shutdown() override { m_bStarted = false; }

        void Update(double InDelta) override { m_Accumulated += InDelta; }

        bool          IsStarted()   const noexcept { return m_bStarted; }
        double        Accumulated() const noexcept { return m_Accumulated; }
        WorldContext* Context()     const noexcept { return m_Context; }

        static int s_StartCount;

    private:
        WorldContext* m_Context     = nullptr;
        bool          m_bStarted    = false;
        double        m_Accumulated = 0.0;
    };

    int AlwaysSubsystem::s_StartCount = 0;

    // -------------------------------------------------------------------------
    // Edit-only and Play-only: one override is the whole cost of being selective.
    // -------------------------------------------------------------------------
    class EditOnlySubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(EditOnlySubsystem, WorldSubsystem)

    public:
        bool ShouldCreate(const World& InWorld) const override { return InWorld.GetMode() == EWorldMode::Edit; }
    };

    class PlayOnlySubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(PlayOnlySubsystem, WorldSubsystem)

    public:
        bool ShouldCreate(const World& InWorld) const override { return InWorld.GetMode() == EWorldMode::Play; }
    };

    // -------------------------------------------------------------------------
    // Ticks first, and finds a sibling that starts after it.
    // -------------------------------------------------------------------------
    class InputFirstSubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(InputFirstSubsystem, WorldSubsystem)

    public:
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Input; }

        bool Startup() override
        {
            Sibling         = GetWorld().GetSubsystems().GetSubsystem<AlwaysSubsystem>();
            bSiblingStarted = (Sibling != nullptr) && Sibling->IsStarted();
            return true;
        }

        AlwaysSubsystem* Sibling         = nullptr;
        bool             bSiblingStarted = false;
    };

    /** A child class: GetSubsystem<Parent>() finds it. */
    class DerivedAlwaysSubsystem final : public AlwaysSubsystem
    {
        OPAAX_OBJECT(DerivedAlwaysSubsystem, AlwaysSubsystem)
    };

    class AbstractWorldSubsystem : public WorldSubsystem
    {
        OPAAX_ABSTRACT_OBJECT(AbstractWorldSubsystem, WorldSubsystem)
    };

    class GameSideSubsystem : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(GameSideSubsystem, GameInstanceSubsystem)
    };

    // -------------------------------------------------------------------------
    // What WorldManager::CreateWorld gives a world: the real engine types, never started. Each is
    // default-constructible and other suites already stack-allocate them, so the context carries
    // valid references. Paths is the engine's inert null object: nothing here loads.
    // -------------------------------------------------------------------------
    struct ContextFixture
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        EngineConfigData Config;
        InputManager     Input;

        void Give(World& InWorld)
        {
            InWorld.SetContext(WorldContext{InWorld, Resources, IPaths::Null(), Events, Input, Config,
                                            /*Actions*/ nullptr, /*UI*/ nullptr, Debug});
        }
    };
}

// =============================================================================
// Finding the classes
// =============================================================================
TEST_CASE("world subsystems: every concrete WorldSubsystem class is found, by tick order then name")
{
    ObjectClassRegistry lClasses;
    lClasses.Register<PlayOnlySubsystem>();
    lClasses.Register<AbstractWorldSubsystem>();
    lClasses.Register<AlwaysSubsystem>();
    lClasses.Register<GameSideSubsystem>();
    lClasses.Register<InputFirstSubsystem>();
    lClasses.Register<EditOnlySubsystem>();

    const TDynArray<const ObjectClass*> lFound = CollectSubsystemClasses(lClasses, WorldSubsystem::StaticClass());

    // Not the abstract base, not the game's subsystem. Input ticks first; the Default ones by name.
    REQUIRE(lFound.size() == 4);
    CHECK(lFound[0] == &InputFirstSubsystem::StaticClass());
    CHECK(lFound[1] == &AlwaysSubsystem::StaticClass());
    CHECK(lFound[2] == &EditOnlySubsystem::StaticClass());
    CHECK(lFound[3] == &PlayOnlySubsystem::StaticClass());
}

TEST_CASE("world subsystems: ShouldCreate on the class default selects by world mode")
{
    World lEditWorld("E", EWorldMode::Edit);
    World lPlayWorld("P", EWorldMode::Play);

    const auto* lEditOnly = Cast<WorldSubsystem>(EditOnlySubsystem::StaticClass().GetDefaultObject());
    const auto* lPlayOnly = Cast<WorldSubsystem>(PlayOnlySubsystem::StaticClass().GetDefaultObject());
    const auto* lAlways   = Cast<WorldSubsystem>(AlwaysSubsystem::StaticClass().GetDefaultObject());

    REQUIRE(lEditOnly != nullptr);
    REQUIRE(lPlayOnly != nullptr);
    REQUIRE(lAlways != nullptr);

    CHECK(lEditOnly->ShouldCreate(lEditWorld));
    CHECK_FALSE(lEditOnly->ShouldCreate(lPlayWorld));

    CHECK_FALSE(lPlayOnly->ShouldCreate(lEditWorld));
    CHECK(lPlayOnly->ShouldCreate(lPlayWorld));

    // No override: made in every world.
    CHECK(lAlways->ShouldCreate(lEditWorld));
    CHECK(lAlways->ShouldCreate(lPlayWorld));
}

// =============================================================================
// Making them in a world — context, order, lifecycle
// =============================================================================
TEST_CASE("world subsystems: a world hands its own context to each subsystem before Startup")
{
    ContextFixture lFixture;
    World          lWorld("Subject", EWorldMode::Play);
    lFixture.Give(lWorld);
    REQUIRE(lWorld.GetContext() != nullptr);

    REQUIRE(lWorld.GetSubsystems().Add(AlwaysSubsystem::StaticClass()));
    lWorld.GetSubsystems().StartupAll();

    AlwaysSubsystem* lSubsystem = lWorld.GetSubsystems().GetSubsystem<AlwaysSubsystem>();
    REQUIRE(lSubsystem != nullptr);
    CHECK(lSubsystem->IsStarted());

    // The address, not a field: it is the world's long-lived context, not a copy.
    CHECK(lSubsystem->Context() == lWorld.GetContext());
    CHECK(&lSubsystem->GetWorld() == &lWorld);

    // Named after its class, for logs.
    CHECK(lSubsystem->GetName() == AlwaysSubsystem::StaticClass().GetName());

    // The tick path WorldManager::Update drives.
    lWorld.GetSubsystems().UpdateAll(0.5);
    CHECK(lSubsystem->Accumulated() == doctest::Approx(0.5));

    // Idempotent shutdown: DestroyWorld calls it, then ~World repeats it.
    lWorld.ShutdownSubsystems();
    CHECK_FALSE(lSubsystem->IsStarted());
    lWorld.ShutdownSubsystems();
}

TEST_CASE("world subsystems: every subsystem exists before any starts, and they start in the order added")
{
    ContextFixture lFixture;
    World          lWorld("Subject", EWorldMode::Play);
    lFixture.Give(lWorld);

    // Added in tick order, as WorldManager does: Input first.
    REQUIRE(lWorld.GetSubsystems().Add(InputFirstSubsystem::StaticClass()));
    REQUIRE(lWorld.GetSubsystems().Add(AlwaysSubsystem::StaticClass()));
    lWorld.GetSubsystems().StartupAll();

    const InputFirstSubsystem* lFirst = lWorld.GetSubsystems().GetSubsystem<InputFirstSubsystem>();
    REQUIRE(lFirst != nullptr);

    // Found during its Startup, before it started itself.
    CHECK(lFirst->Sibling == lWorld.GetSubsystems().GetSubsystem<AlwaysSubsystem>());
    CHECK_FALSE(lFirst->bSiblingStarted);

    lWorld.ShutdownSubsystems();
}

TEST_CASE("world subsystems: the class default object is never started")
{
    AlwaysSubsystem::s_StartCount = 0;

    ContextFixture lFixture;
    World          lWorld("Subject", EWorldMode::Play);
    lFixture.Give(lWorld);

    // Asked for ShouldCreate first, as WorldManager does: the default exists from here on.
    REQUIRE(AlwaysSubsystem::StaticClass().GetDefaultObject() != nullptr);

    REQUIRE(lWorld.GetSubsystems().Add(AlwaysSubsystem::StaticClass()));
    lWorld.GetSubsystems().StartupAll();

    CHECK(AlwaysSubsystem::s_StartCount == 1);

    const auto* lDefault = static_cast<const AlwaysSubsystem*>(AlwaysSubsystem::StaticClass().GetDefaultObject());
    CHECK_FALSE(lDefault->IsStarted());
    CHECK(lDefault->Context() == nullptr);

    lWorld.ShutdownSubsystems();
}

TEST_CASE("world subsystems: GetSubsystem finds a child class through its parent")
{
    ContextFixture lFixture;
    World          lWorld("Subject", EWorldMode::Play);
    lFixture.Give(lWorld);

    REQUIRE(lWorld.GetSubsystems().Add(DerivedAlwaysSubsystem::StaticClass()));
    lWorld.GetSubsystems().StartupAll();

    const DerivedAlwaysSubsystem* lDerived = lWorld.GetSubsystems().GetSubsystem<DerivedAlwaysSubsystem>();
    REQUIRE(lDerived != nullptr);

    CHECK(lWorld.GetSubsystems().GetSubsystem<AlwaysSubsystem>() == lDerived);
    CHECK(lWorld.GetSubsystems().GetSubsystem<PlayOnlySubsystem>() == nullptr);

    lWorld.ShutdownSubsystems();
}

TEST_CASE("world subsystems: a world refuses an abstract class and another kind's class")
{
    World lWorld("Subject", EWorldMode::Play);

    CHECK_FALSE(lWorld.GetSubsystems().Add(WorldSubsystem::StaticClass()));
    CHECK_FALSE(lWorld.GetSubsystems().Add(AbstractWorldSubsystem::StaticClass()));
    CHECK_FALSE(lWorld.GetSubsystems().Add(GameSideSubsystem::StaticClass()));

    lWorld.GetSubsystems().StartupAll();
    CHECK(lWorld.GetSubsystems().GetSystems().empty());
}

// =============================================================================
// The bare-manager case, asserted rather than worked around
// =============================================================================
TEST_CASE("world subsystems: a manager with no engine creates a world with no subsystems")
{
    // WorldManager::Startup resolves its engine siblings; a bare manager never started, so there is
    // no context to give subsystems. A world with an empty subsystem list is the correct outcome,
    // and why the cases above add the classes to a world directly.
    WorldManager lWorlds;

    World* lWorld = lWorlds.CreateWorld("Bare", EWorldMode::Play);

    REQUIRE(lWorld != nullptr);
    CHECK(lWorld->GetSubsystems().GetSystems().empty());
    CHECK(lWorld->GetContext() == nullptr);
}
