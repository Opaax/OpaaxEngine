// Suite: the game lifecycle — StartGame / EndGame, the GameInstance (and a game's own class) and its
// subsystem classes.
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
#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Registries/ModuleRegistrar.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/ResourceManager.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    /** The classes of the subsystems started, in start order. */
    TDynArray<OpaaxStringID>& StartOrder()
    {
        static TDynArray<OpaaxStringID> s_Order;
        return s_Order;
    }

    // -------------------------------------------------------------------------
    // Records the context address it was given at Startup (comparing a field could pass against
    // freed memory).
    // -------------------------------------------------------------------------
    class ProbeSubsystem : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(ProbeSubsystem, GameInstanceSubsystem)

    public:
        bool Startup() override
        {
            m_Context  = &GetContext();
            m_bStarted = true;
            ++s_StartCount;
            StartOrder().push_back(GetClass().GetName());
            return true;
        }

        void Shutdown() override { m_bStarted = false; ++s_ShutdownCount; }
        void TearDown() override { ++s_TearDownCount; }

        void Update(double InDelta) override { m_Accumulated += InDelta; }

        bool                 IsStarted()   const noexcept { return m_bStarted; }
        double               Accumulated() const noexcept { return m_Accumulated; }
        GameInstanceContext* Context()     const noexcept { return m_Context; }

        static void Reset() { s_StartCount = 0; s_ShutdownCount = 0; s_TearDownCount = 0; StartOrder().clear(); }

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

    /** A second class with a lower tick order: made and started before the probe. */
    class SecondSubsystem : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(SecondSubsystem, GameInstanceSubsystem)

    public:
        Int32 GetTickOrder() const override { return -10; }

        bool Startup() override
        {
            m_Context = &GetContext();
            StartOrder().push_back(GetClass().GetName());
            return true;
        }

        GameInstanceContext* Context() const noexcept { return m_Context; }

    private:
        GameInstanceContext* m_Context = nullptr;
    };

    /** A base no game makes by itself. */
    class AbstractGameSubsystem : public GameInstanceSubsystem
    {
        OPAAX_ABSTRACT_OBJECT(AbstractGameSubsystem, GameInstanceSubsystem)
    };

    /** A world's subsystem: a game never makes it. */
    class OtherKindSubsystem : public WorldSubsystem
    {
        OPAAX_OBJECT(OtherKindSubsystem, WorldSubsystem)
    };

    /** A game's own class: what it saw when its hooks ran. */
    class StoryGame : public GameInstance
    {
        OPAAX_OBJECT(StoryGame, GameInstance)

    public:
        int  StartCount           = 0;
        int  EndCount             = 0;
        bool bProbeRunningAtStart = false;
        bool bProbeRunningAtEnd   = false;

    protected:
        void OnStart() override
        {
            ++StartCount;
            const ProbeSubsystem* lProbe = GetSubsystems().GetSubsystem<ProbeSubsystem>();
            bProbeRunningAtStart = lProbe != nullptr && lProbe->IsStarted();
        }

        void OnEnd() override
        {
            ++EndCount;
            const ProbeSubsystem* lProbe = GetSubsystems().GetSubsystem<ProbeSubsystem>();
            bProbeRunningAtEnd = lProbe != nullptr && lProbe->IsStarted() && ProbeSubsystem::s_TearDownCount == 0;
        }
    };

    /** A base no project runs by itself. */
    class AbstractGame : public GameInstance
    {
        OPAAX_ABSTRACT_OBJECT(AbstractGame, GameInstance)
    };

    /** The classes a game finds: the two probes, and the two it must leave out. */
    void RegisterClasses(ObjectClassRegistry& OutClasses)
    {
        OutClasses.Register<ProbeSubsystem>();
        OutClasses.Register<AbstractGameSubsystem>();
        OutClasses.Register<OtherKindSubsystem>();
        OutClasses.Register<SecondSubsystem>();
    }

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
TEST_SUITE("GameInstanceSubsystem classes")
{
    TEST_CASE("A game takes every concrete GameInstanceSubsystem class, by tick order then name")
    {
        ObjectClassRegistry lClasses;
        RegisterClasses(lClasses);

        const TDynArray<const ObjectClass*> lFound =
            CollectSubsystemClasses(lClasses, GameInstanceSubsystem::StaticClass());

        // The abstract base and the world's subsystem are left out; Second's lower order puts it first.
        REQUIRE(lFound.size() == 2);
        CHECK(lFound[0] == &SecondSubsystem::StaticClass());
        CHECK(lFound[1] == &ProbeSubsystem::StaticClass());
    }

    TEST_CASE("Classes of another base are ordered by their own tick order")
    {
        ObjectClassRegistry lClasses;
        lClasses.Register<OtherKindSubsystem>();
        lClasses.Register<ProbeSubsystem>();

        // Asked for every subsystem: a game subsystem's default order (0) is lower than a world
        // subsystem's (Default), whatever the registration order.
        const TDynArray<const ObjectClass*> lFound = CollectSubsystemClasses(lClasses, Subsystem::StaticClass());

        REQUIRE(lFound.size() == 2);
        CHECK(lFound[0] == &ProbeSubsystem::StaticClass());
        CHECK(lFound[1] == &OtherKindSubsystem::StaticClass());
    }
}

// =============================================================================
TEST_SUITE("GameInstance")
{
    TEST_CASE("Start makes one subsystem per class, starts them in tick order, and hands each the SAME context")
    {
        ProbeSubsystem::Reset();

        ObjectClassRegistry lClasses;
        RegisterClasses(lClasses);

        ContextFixture lFixture;
        GameInstance   lGame;
        lGame.Init(lFixture.Make());

        CHECK(lGame.Start(lClasses) == 2);
        CHECK(lGame.GetSubsystemCount() == 2);
        CHECK(lGame.IsRunning());

        ProbeSubsystem*  lProbe  = lGame.GetSubsystems().GetSubsystem<ProbeSubsystem>();
        SecondSubsystem* lSecond = lGame.GetSubsystems().GetSubsystem<SecondSubsystem>();

        REQUIRE(lProbe != nullptr);
        REQUIRE(lSecond != nullptr);
        CHECK(lProbe->IsStarted());
        CHECK(ProbeSubsystem::s_StartCount == 1);

        // Same address: the context is the GameInstance's own stable slot.
        CHECK(lProbe->Context() == &lGame.GetContext());
        CHECK(lSecond->Context() == &lGame.GetContext());

        REQUIRE(StartOrder().size() == 2);
        CHECK(StartOrder()[0] == SecondSubsystem::StaticClass().GetName());
        CHECK(StartOrder()[1] == ProbeSubsystem::StaticClass().GetName());

        // Named after its class, for logs.
        CHECK(lProbe->GetName() == ProbeSubsystem::StaticClass().GetName());

        lGame.End();
    }

    TEST_CASE("Start is refused before Init, and the second time")
    {
        ProbeSubsystem::Reset();

        ObjectClassRegistry lClasses;
        lClasses.Register<ProbeSubsystem>();

        ContextFixture lFixture;
        GameInstance   lGame;

        // No services yet: nothing is made.
        CHECK(lGame.Start(lClasses) == 0);
        CHECK_FALSE(lGame.IsRunning());
        CHECK(ProbeSubsystem::s_StartCount == 0);

        lGame.Init(lFixture.Make());
        CHECK(lGame.Start(lClasses) == 1);
        CHECK(lGame.Start(lClasses) == 0);
        CHECK(lGame.GetSubsystemCount() == 1);
        CHECK(ProbeSubsystem::s_StartCount == 1);

        lGame.End();
    }

    TEST_CASE("Update reaches the subsystems")
    {
        ObjectClassRegistry lClasses;
        lClasses.Register<ProbeSubsystem>();

        ContextFixture lFixture;
        GameInstance   lGame;
        lGame.Init(lFixture.Make());
        lGame.Start(lClasses);

        ProbeSubsystem* lProbe = lGame.GetSubsystems().GetSubsystem<ProbeSubsystem>();
        REQUIRE(lProbe != nullptr);

        lGame.Update(0.5);
        lGame.Update(0.25);
        CHECK(lProbe->Accumulated() == doctest::Approx(0.75));

        lGame.End();
    }

    TEST_CASE("End tears down and shuts down once, however often it is called")
    {
        ProbeSubsystem::Reset();

        ObjectClassRegistry lClasses;
        lClasses.Register<ProbeSubsystem>();

        ContextFixture lFixture;
        {
            GameInstance lGame;
            lGame.Init(lFixture.Make());
            lGame.Start(lClasses);

            lGame.End();
            lGame.End();

            CHECK_FALSE(lGame.IsRunning());
            CHECK(ProbeSubsystem::s_TearDownCount == 1);
            CHECK(ProbeSubsystem::s_ShutdownCount == 1);
        }

        // The destructor must not shut a subsystem down twice.
        CHECK(ProbeSubsystem::s_ShutdownCount == 1);
    }

    TEST_CASE("A game destroyed without End shuts its subsystems down, without tearing them down")
    {
        ProbeSubsystem::Reset();

        ObjectClassRegistry lClasses;
        lClasses.Register<ProbeSubsystem>();

        ContextFixture lFixture;
        {
            GameInstance lGame;
            lGame.Init(lFixture.Make());
            lGame.Start(lClasses);
        }

        // TearDown may use services already gone; Shutdown is the safety net.
        CHECK(ProbeSubsystem::s_TearDownCount == 0);
        CHECK(ProbeSubsystem::s_ShutdownCount == 1);
    }

    TEST_CASE("A game's own class: OnStart once its subsystems run, OnEnd before they tear down")
    {
        ProbeSubsystem::Reset();

        ObjectClassRegistry lClasses;
        lClasses.Register<ProbeSubsystem>();

        ContextFixture lFixture;
        StoryGame      lGame;
        lGame.Init(lFixture.Make());
        lGame.Start(lClasses);

        CHECK(lGame.StartCount == 1);
        CHECK(lGame.bProbeRunningAtStart);
        CHECK(lGame.EndCount == 0);

        lGame.End();
        lGame.End();
        CHECK(lGame.EndCount == 1);
        CHECK(lGame.bProbeRunningAtEnd);
        CHECK(ProbeSubsystem::s_TearDownCount == 1);
    }

    TEST_CASE("A game that never started has nothing to end: no OnEnd")
    {
        StoryGame lGame;
        lGame.End();

        CHECK(lGame.StartCount == 0);
        CHECK(lGame.EndCount == 0);
    }

    TEST_CASE("Create makes the class named; no name, or one that is not a concrete GameInstance class, gives a GameInstance")
    {
        ObjectClassRegistry lClasses;
        lClasses.Register<GameInstance>();
        lClasses.Register<StoryGame>();
        lClasses.Register<AbstractGame>();
        lClasses.Register<ProbeSubsystem>();

        const auto lClassOf = [&lClasses](const OpaaxStringID InName) -> const ObjectClass*
        {
            const TStrongObjectPtr<GameInstance> lGame = GameInstance::Create(InName, lClasses);
            return lGame ? &lGame->GetClass() : nullptr;
        };

        CHECK(lClassOf(OPAAX_ID("StoryGame")) == &StoryGame::StaticClass());
        CHECK(lClassOf(OpaaxStringID()) == &GameInstance::StaticClass());
        CHECK(lClassOf(OPAAX_ID("NoSuchGame")) == &GameInstance::StaticClass());
        CHECK(lClassOf(OPAAX_ID("AbstractGame")) == &GameInstance::StaticClass());
        CHECK(lClassOf(OPAAX_ID("ProbeSubsystem")) == &GameInstance::StaticClass());
    }

    TEST_CASE("GameInstance is an object class, registered by the engine under its name")
    {
        CHECK(GameInstance::StaticClass().GetName() == OPAAX_ID("GameInstance"));
        CHECK(GameInstance::StaticClass().IsChildOf(Object::StaticClass()));
        CHECK_FALSE(GameInstance::StaticClass().IsAbstract());

        EngineRegistries lRegistries;
        ModuleRegistrar  lRegistrar;
        lRegistrar.BindEngineRegistries(lRegistries);
        RunAutoRegistrations(lRegistrar);

        CHECK(lRegistries.ObjectClasses().FindByName(OPAAX_ID("GameInstance")) == &GameInstance::StaticClass());
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
        lRegistries.ObjectClasses().Register<ProbeSubsystem>();

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

    TEST_CASE("Makes the game of the class it is given: a GameInstance when none is named")
    {
        ProbeSubsystem::Reset();

        EngineRegistries lRegistries;
        lRegistries.ObjectClasses().Register<GameInstance>();
        lRegistries.ObjectClasses().Register<StoryGame>();

        GameInstanceManager lManager(&lRegistries);
        REQUIRE(lManager.Startup());

        // No project in a test, so no class named: a plain game.
        CHECK_FALSE(lManager.GetGameInstanceClass().IsValid());
        REQUIRE(lManager.StartGame());
        CHECK(&lManager.GetGameInstance()->GetClass() == &GameInstance::StaticClass());
        CHECK(lManager.EndGame());

        // What a project's gameInstanceClass sets at Startup.
        lManager.SetGameInstanceClass(OPAAX_ID("StoryGame"));
        REQUIRE(lManager.StartGame());

        StoryGame* lStory = Cast<StoryGame>(lManager.GetGameInstance());
        REQUIRE(lStory != nullptr);
        CHECK(lStory->StartCount == 1);
        CHECK(lStory->IsRunning());

        CHECK(lManager.EndGame());
        CHECK(lManager.GetGameInstance() == nullptr);
    }

    TEST_CASE("A second game is a FRESH instance — nothing leaks across a PIE cycle")
    {
        ProbeSubsystem::Reset();

        EngineRegistries lRegistries;
        lRegistries.ObjectClasses().Register<ProbeSubsystem>();

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
