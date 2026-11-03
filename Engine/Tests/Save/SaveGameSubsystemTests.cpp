// Suite: SaveGameSubsystem — a game's saves through its subsystem, started the way a game starts it
// (a GameInstance with a context), with a save storage in a temp directory: saving and loading across
// games, what each failure answers, and the slots.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ISaveStorage.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Registries/ModuleRegistrar.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/ResourceManager.h"
#include "Save/SaveGame.h"
#include "Save/SaveGameSubsystem.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace SaveSubsystemTestTypes
{
    class TestRunSave final : public SaveGame
    {
        OPAAX_OBJECT(TestRunSave, SaveGame)

    public:
        Int32 Coins    = 0;
        float BestTime = 0.f;

        OPAAX_PROPERTIES(TestRunSave, OPAAX_PROP(Coins), OPAAX_PROP(BestTime))
    };

    class TestOptionsSave final : public SaveGame
    {
        OPAAX_OBJECT(TestOptionsSave, SaveGame)

    public:
        float Volume = 1.f;
        OPAAX_PROPERTIES(TestOptionsSave, OPAAX_PROP(Volume))
    };
}

using namespace SaveSubsystemTestTypes;

namespace
{
    // -------------------------------------------------------------------------
    // What a game's context needs, with the player's slots in a temp directory.
    // -------------------------------------------------------------------------
    struct SaveFixture
    {
        TestTempDir         Temp{ "save_subsystem" };
        SaveStorage         Storage{ Temp.Sub("SaveGames") };
        ObjectClassRegistry Classes;

        WorldManager     Worlds;
        ResourceManager  Resources;
        EngineEventBus   Events;
        InputManager     Input;
        EngineConfigData Config;

        SaveFixture()
        {
            Classes.Register<SaveGame>();
            Classes.Register<TestRunSave>();
            Classes.Register<TestOptionsSave>();
            Resources.SetObjectClasses(&Classes);
        }

        ~SaveFixture()
        {
            EndGame();
        }

        /** A new game, its SaveGameSubsystem started the way a game starts it. */
        SaveGameSubsystem& StartGame(IJobSystem& InJobs = IJobSystem::Null())
        {
            EndGame();

            Game = NewObject<GameInstance>();
            Game->Init(GameInstanceContext{ Worlds, Resources, IPaths::Null(), Events, Input, Config, InJobs, Storage });
            Game->GetSubsystems().Add<SaveGameSubsystem>();
            Game->GetSubsystems().StartupAll();
            return *Game->GetSubsystems().GetSubsystem<SaveGameSubsystem>();
        }

        void EndGame()
        {
            if (Game.IsValid())
            {
                Game->End();
                Game.Reset();
            }
        }

        // Last: the game ends before the services its context names.
        TStrongObjectPtr<GameInstance> Game;
    };
}

// =============================================================================
// Every game has one
// =============================================================================

TEST_CASE("SaveGameSubsystem: the engine registers it, so every game makes one")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);
    RunAutoRegistrations(lRegistrar);

    CHECK(lRegistries.ObjectClasses().FindByName(OPAAX_ID("SaveGame")) == &SaveGame::StaticClass());

    bool bFound = false;
    for (const ObjectClass* lClass : CollectSubsystemClasses(lRegistries.ObjectClasses(),
                                                             GameInstanceSubsystem::StaticClass()))
    {
        bFound = bFound || lClass == &SaveGameSubsystem::StaticClass();
    }
    CHECK(bFound);
}

// =============================================================================
// Saving and loading
// =============================================================================

TEST_CASE("SaveGameSubsystem: a save made in one game loads in the next")
{
    SaveFixture lFixture;

    {
        SaveGameSubsystem& lSaves = lFixture.StartGame();

        const TStrongObjectPtr<TestRunSave> lSave = NewObject<TestRunSave>();
        lSave->Coins    = 42;
        lSave->BestTime = 12.5f;
        CHECK(lSaves.Save("Main", *lSave) == ESaveResult::Ok);
    }

    SaveGameSubsystem& lSaves = lFixture.StartGame();

    TStrongObjectPtr<TestRunSave> lLoaded;
    REQUIRE(lSaves.Load("Main", lLoaded) == ESaveResult::Ok);
    REQUIRE(lLoaded.IsValid());
    CHECK(lLoaded->GetName() == OPAAX_ID("Main"));
    CHECK(lLoaded->Coins    == 42);
    CHECK(lLoaded->BestTime == 12.5f);

    // Any save class reads it as itself.
    TStrongObjectPtr<SaveGame> lAny;
    REQUIRE(lSaves.Load("Main", lAny) == ESaveResult::Ok);
    CHECK(lAny->IsA<TestRunSave>());
}

TEST_CASE("SaveGameSubsystem: only a slot without a save is Missing; one it cannot read says why")
{
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame();

    REQUIRE(lFixture.Storage.Write("Broken", "{ \"Class\": \"TestRunSave\", \"Prop") == ESaveResult::Ok);
    REQUIRE(lFixture.Storage.Write("Newer", R"({ "Class": "TestRunSave", "ClassVersion": 9 })") == ESaveResult::Ok);
    REQUIRE(lFixture.Storage.Write("Options", R"({ "Class": "TestOptionsSave" })") == ESaveResult::Ok);

    // A pointer the game already holds is left as it was.
    const TStrongObjectPtr<TestRunSave> lHeld = NewObject<TestRunSave>();
    TStrongObjectPtr<TestRunSave>       lSave = lHeld;

    CHECK(lSaves.Load("Main", lSave)      == ESaveResult::Missing);
    CHECK(lSaves.Load("Broken", lSave)    == ESaveResult::Corrupt);
    CHECK(lSaves.Load("Newer", lSave)     == ESaveResult::NewerVersion);
    CHECK(lSaves.Load("Options", lSave)   == ESaveResult::WrongClass);
    CHECK(lSaves.Load("../Main", lSave)   == ESaveResult::InvalidSlot);
    CHECK(lSave.Get() == lHeld.Get());

    // Nothing was written over.
    OpaaxString lText;
    REQUIRE(lFixture.Storage.Read("Newer", lText) == ESaveResult::Ok);
    CHECK(lText == R"({ "Class": "TestRunSave", "ClassVersion": 9 })");
}

TEST_CASE("SaveGameSubsystem: Exists, Delete and ListSlots answer from the player's slots")
{
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame();

    const TStrongObjectPtr<TestRunSave>     lRun     = NewObject<TestRunSave>();
    const TStrongObjectPtr<TestOptionsSave> lOptions = NewObject<TestOptionsSave>();
    REQUIRE(lSaves.Save("Main", *lRun) == ESaveResult::Ok);
    REQUIRE(lSaves.Save("Options", *lOptions) == ESaveResult::Ok);

    CHECK(lSaves.Exists("Main"));
    CHECK_FALSE(lSaves.Exists("Other"));

    const TDynArray<OpaaxString> lSlots = lSaves.ListSlots();
    REQUIRE(lSlots.size() == 2);
    CHECK(lSlots[0] == "Main");
    CHECK(lSlots[1] == "Options");

    CHECK(lSaves.Delete("Main") == ESaveResult::Ok);
    CHECK_FALSE(lSaves.Exists("Main"));
    CHECK(lSaves.Delete("Main") == ESaveResult::Missing);
    CHECK(lSaves.ListSlots().size() == 1);
}

TEST_CASE("SaveGameSubsystem: a save that cannot be written keeps the slot's last save")
{
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame();

    const TStrongObjectPtr<TestRunSave> lSave = NewObject<TestRunSave>();
    lSave->Coins = 1;
    REQUIRE(lSaves.Save("Main", *lSave) == ESaveResult::Ok);

    // A folder where the write's temporary file goes: the disk refuses the next write.
    REQUIRE(FileSystem::CreateDirectories(FileIO::AtomicTempPath(lFixture.Storage.GetSlotPath("Main"))));

    lSave->Coins = 2;
    CHECK(lSaves.Save("Main", *lSave) == ESaveResult::IoError);
    CHECK(lSaves.Save("no slot", *lSave) == ESaveResult::InvalidSlot);

    TStrongObjectPtr<TestRunSave> lLoaded;
    REQUIRE(lSaves.Load("Main", lLoaded) == ESaveResult::Ok);
    CHECK(lLoaded->Coins == 1);
}
