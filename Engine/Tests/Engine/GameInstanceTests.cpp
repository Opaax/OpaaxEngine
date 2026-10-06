// Suite: the game lifecycle — StartGame / EndGame, the GameInstance and its registry.
//   StartGame -> GameInstance -> first world -> ... -> EndGame -> destroy Play worlds -> destroy
//   the GameInstance. The game must exist before the first world (world contexts are built at
//   creation). Where the hosts call these is checked by their boot log, not here.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/GameInstance/GameInstanceSubsystemRegistry.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    OpaaxStringID Name(const char* InText) { return OpaaxStringID(OpaaxString(InText)); }

    // -------------------------------------------------------------------------
    // Records the context address it was given (comparing a field could pass against freed memory).
    // -------------------------------------------------------------------------
    class ProbeSubsystem : public GameInstanceSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(ProbeSubsystem)

        explicit ProbeSubsystem(GameInstanceContext& InContext) : m_Context(&InContext) {}

        bool Startup() override  { m_bStarted = true;  ++s_StartCount; return true; }
        void Shutdown() override { m_bStarted = false; ++s_ShutdownCount; }
        void TearDown() override { ++s_TearDownCount; }

        void Update(double InDelta) override { m_Accumulated += InDelta; }

        bool                 IsStarted()   const noexcept { return m_bStarted; }
        double               Accumulated() const noexcept { return m_Accumulated; }
        GameInstanceContext* Context()     const noexcept { return m_Context; }

        static void Reset() { s_StartCount = 0; s_ShutdownCount = 0; s_TearDownCount = 0; }

        static int s_StartCount;
        static int s_ShutdownCount;
        static int s_TearDownCount;

    private:
        GameInstanceContext* m_Context     = nullptr;
        bool                 m_bStarted    = false;
        double               m_Accumulated = 0.0;
    };

    int ProbeSubsystem::s_StartCount    = 0;
    int ProbeSubsystem::s_ShutdownCount = 0;
    int ProbeSubsystem::s_TearDownCount = 0;

    /** A second type, so "created in registration order" is a statement about more than one. */
    class SecondSubsystem : public GameInstanceSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(SecondSubsystem)

        explicit SecondSubsystem(GameInstanceContext& InContext) : m_Context(&InContext) {}

        bool Startup() override  { return true; }
        void Shutdown() override {}

        GameInstanceContext* Context() const noexcept { return m_Context; }

    private:
        GameInstanceContext* m_Context = nullptr;
    };

    // -------------------------------------------------------------------------
    // The engine-side half of a context, owned by the fixture so a GameInstance can
    // borrow it exactly as GameInstanceManager::StartGame does.
    // -------------------------------------------------------------------------
    struct ContextFixture
    {
        WorldManager    Worlds;
        ResourceManager Resources;
        EngineEventBus  Events;
        InputManager    Input;
        EngineConfigData Config;

        GameInstanceContext Make()
        {
            return GameInstanceContext{Worlds, Resources, IPaths::Null(), Events, Input, Config};
        }
    };
}

// =============================================================================
TEST_SUITE("GameInstanceSubsystemRegistry")
{
    TEST_CASE("Registers candidates and counts them in order")
    {
        GameInstanceSubsystemRegistry lRegistry;

        CHECK(lRegistry.Count() == 0);
        CHECK(lRegistry.Register<ProbeSubsystem>(Name("Probe")));
        CHECK(lRegistry.Register<SecondSubsystem>(Name("Second")));
        CHECK(lRegistry.Count() == 2);

        CHECK(lRegistry.FindByName(Name("Probe")) != nullptr);
        CHECK(lRegistry.FindByName(Name("Second")) != nullptr);
        CHECK(lRegistry.FindByName(Name("Absent")) == nullptr);
    }

    TEST_CASE("Refuses an empty name and a duplicate name")
    {
        GameInstanceSubsystemRegistry lRegistry;

        CHECK_FALSE(lRegistry.Register<ProbeSubsystem>(OpaaxStringID{}));
        CHECK(lRegistry.Count() == 0);

        CHECK(lRegistry.Register<ProbeSubsystem>(Name("Probe")));
        CHECK_FALSE(lRegistry.Register<SecondSubsystem>(Name("Probe")));
        CHECK(lRegistry.Count() == 1);
    }

    TEST_CASE("Sealing closes registration, and is idempotent")
    {
        GameInstanceSubsystemRegistry lRegistry;

        CHECK(lRegistry.Register<ProbeSubsystem>(Name("Probe")));
        CHECK_FALSE(lRegistry.IsSealed());

        lRegistry.Seal();
        lRegistry.Seal();
        CHECK(lRegistry.IsSealed());

        // A candidate accepted now would simply be absent from a game that already exists.
        CHECK_FALSE(lRegistry.Register<SecondSubsystem>(Name("Second")));
        CHECK(lRegistry.Count() == 1);
    }
}

// =============================================================================
TEST_SUITE("GameInstance")
{
    TEST_CASE("Creates every candidate, starts them, and hands each the SAME context")
    {
        ProbeSubsystem::Reset();

        GameInstanceSubsystemRegistry lRegistry;
        lRegistry.Register<ProbeSubsystem>(Name("Probe"));
        lRegistry.Register<SecondSubsystem>(Name("Second"));

        ContextFixture lFixture;
        GameInstance   lGame(lFixture.Make());

        CHECK(lGame.StartSubsystems(lRegistry) == 2);
        CHECK(lGame.GetSubsystemCount() == 2);

        ProbeSubsystem*  lProbe  = lGame.GetSubsystems().GetSubsystem<ProbeSubsystem>();
        SecondSubsystem* lSecond = lGame.GetSubsystems().GetSubsystem<SecondSubsystem>();

        REQUIRE(lProbe != nullptr);
        REQUIRE(lSecond != nullptr);
        CHECK(lProbe->IsStarted());
        CHECK(ProbeSubsystem::s_StartCount == 1);

        // Same address: the context is the GameInstance's own stable slot.
        CHECK(lProbe->Context() == &lGame.GetContext());
        CHECK(lSecond->Context() == &lGame.GetContext());
    }

    TEST_CASE("Update reaches the subsystems")
    {
        GameInstanceSubsystemRegistry lRegistry;
        lRegistry.Register<ProbeSubsystem>(Name("Probe"));

        ContextFixture lFixture;
        GameInstance   lGame(lFixture.Make());
        lGame.StartSubsystems(lRegistry);

        ProbeSubsystem* lProbe = lGame.GetSubsystems().GetSubsystem<ProbeSubsystem>();
        REQUIRE(lProbe != nullptr);

        lGame.Update(0.5);
        lGame.Update(0.25);
        CHECK(lProbe->Accumulated() == doctest::Approx(0.75));
    }

    TEST_CASE("TearDown and Shutdown are idempotent")
    {
        ProbeSubsystem::Reset();

        GameInstanceSubsystemRegistry lRegistry;
        lRegistry.Register<ProbeSubsystem>(Name("Probe"));

        ContextFixture lFixture;
        {
            GameInstance lGame(lFixture.Make());
            lGame.StartSubsystems(lRegistry);

            lGame.TearDownSubsystems();
            lGame.TearDownSubsystems();
            lGame.ShutdownSubsystems();
            lGame.ShutdownSubsystems();

            CHECK(ProbeSubsystem::s_TearDownCount == 1);
            CHECK(ProbeSubsystem::s_ShutdownCount == 1);
        }

        // The destructor calls ShutdownSubsystems again; it must not shut a subsystem down twice.
        CHECK(ProbeSubsystem::s_ShutdownCount == 1);
    }
}

// =============================================================================
TEST_SUITE("GameInstanceManager")
{
    TEST_CASE("Refuses to start a game with no registries")
    {
        GameInstanceManager lManager;   // no registries — a bare manager in a test

        CHECK_FALSE(lManager.StartGame());
        CHECK_FALSE(lManager.IsGameRunning());
        CHECK(lManager.GetGameInstance() == nullptr);
    }

    TEST_CASE("EndGame with no game running is a silent no-op")
    {
        GameInstanceManager lManager;

        // False, not an error: hosts call it unconditionally.
        CHECK_FALSE(lManager.EndGame());
        CHECK_FALSE(lManager.IsGameRunning());
    }

    TEST_CASE("Starts a game, refuses a second, and ends it")
    {
        ProbeSubsystem::Reset();

        EngineRegistries lRegistries;
        lRegistries.GameInstanceSubsystems().Register<ProbeSubsystem>(Name("Probe"));

        GameInstanceManager lManager(&lRegistries);
        REQUIRE(lManager.Startup());

        REQUIRE(lManager.StartGame());
        CHECK(lManager.IsGameRunning());

        GameInstance* lGame = lManager.GetGameInstance();
        REQUIRE(lGame != nullptr);
        CHECK(lGame->GetSubsystemCount() == 1);
        CHECK(ProbeSubsystem::s_StartCount == 1);

        // A second StartGame is a caller mistake: refuse it loudly.
        CHECK_FALSE(lManager.StartGame());
        CHECK(lManager.GetGameInstance() == lGame);
        CHECK(ProbeSubsystem::s_StartCount == 1);

        CHECK(lManager.EndGame());
        CHECK_FALSE(lManager.IsGameRunning());
        CHECK(lManager.GetGameInstance() == nullptr);
        CHECK(ProbeSubsystem::s_TearDownCount == 1);
        CHECK(ProbeSubsystem::s_ShutdownCount == 1);
    }

    TEST_CASE("A second game is a FRESH instance — nothing leaks across a PIE cycle")
    {
        ProbeSubsystem::Reset();

        EngineRegistries lRegistries;
        lRegistries.GameInstanceSubsystems().Register<ProbeSubsystem>(Name("Probe"));

        GameInstanceManager lManager(&lRegistries);
        REQUIRE(lManager.Startup());

        REQUIRE(lManager.StartGame());
        lManager.Update(1.0);
        CHECK(lManager.EndGame());

        REQUIRE(lManager.StartGame());
        GameInstance* lSecond = lManager.GetGameInstance();
        REQUIRE(lSecond != nullptr);

        ProbeSubsystem* lProbe = lSecond->GetSubsystems().GetSubsystem<ProbeSubsystem>();
        REQUIRE(lProbe != nullptr);

        // Rebuilt, not reset: the first game's state is gone with its destroyed subsystem.
        CHECK(ProbeSubsystem::s_StartCount == 2);
        CHECK(lProbe->Accumulated() == doctest::Approx(0.0));

        // The counter the log prints as "session #N".
        CHECK(lManager.GetSessionsStarted() == 2);

        CHECK(lManager.EndGame());
    }
}

// =============================================================================
TEST_SUITE("WorldManager::DestroyWorldsOfMode")
{
    TEST_CASE("Destroys Play worlds and leaves the Edit world alone")
    {
        WorldManager lWorlds;   // bare: no registries, so worlds get no subsystems (documented)

        World* lEdit = lWorlds.CreateWorld("EditWorld", EWorldMode::Edit);
        lWorlds.CreateWorld("PlayA", EWorldMode::Play);
        lWorlds.CreateWorld("PlayB", EWorldMode::Play);
        REQUIRE(lWorlds.GetWorldCount() == 3);

        lWorlds.SetActiveWorld(lEdit);

        // EndGame's mechanism: the Play copy goes, the edit world stays.
        CHECK(lWorlds.DestroyWorldsOfMode(EWorldMode::Play) == 2);
        CHECK(lWorlds.GetWorldCount() == 1);
        CHECK(lWorlds.GetActiveWorld() == lEdit);
    }

    TEST_CASE("No matching world destroys nothing")
    {
        WorldManager lWorlds;

        lWorlds.CreateWorld("EditWorld", EWorldMode::Edit);

        CHECK(lWorlds.DestroyWorldsOfMode(EWorldMode::Play) == 0);
        CHECK(lWorlds.GetWorldCount() == 1);
    }

    TEST_CASE("IsActive tracks the active slot, and only ONE world holds it")
    {
        WorldManager lWorlds;

        World* lFirst  = lWorlds.CreateWorld("First", EWorldMode::Play);
        World* lSecond = lWorlds.CreateWorld("Second", EWorldMode::Play);

        // Created is not activated — the startup world is created before anything activates it.
        CHECK_FALSE(lFirst->IsActive());
        CHECK_FALSE(lSecond->IsActive());

        lWorlds.SetActiveWorld(lFirst);
        CHECK(lFirst->IsActive());
        CHECK_FALSE(lSecond->IsActive());

        // Level change: both worlds exist and are Play; only the active flag tells them apart.
        lWorlds.SetActiveWorld(lSecond);
        CHECK_FALSE(lFirst->IsActive());
        CHECK(lSecond->IsActive());

        lWorlds.DestroyWorld(lSecond);
        CHECK(lWorlds.GetActiveWorld() == nullptr);
    }

    TEST_CASE("CountWorldsOfMode separates Edit from Play")
    {
        WorldManager lWorlds;

        CHECK(lWorlds.CountWorldsOfMode(EWorldMode::Play) == 0);

        lWorlds.CreateWorld("EditWorld", EWorldMode::Edit);
        lWorlds.CreateWorld("PlayA", EWorldMode::Play);

        // InputMappingSubsystem::Startup logs this: the total is 1 in the editor either way, only the
        // Play count tells a correct boot order from a wrong one.
        CHECK(lWorlds.CountWorldsOfMode(EWorldMode::Play) == 1);
        CHECK(lWorlds.CountWorldsOfMode(EWorldMode::Edit) == 1);
        CHECK(lWorlds.GetWorldCount() == 2);
    }
}
