// Suite: SaveGameSubsystem — a game's saves through its subsystem, started the way a game starts it
// (a GameInstance with a context), with a save storage in a temp directory: saving and loading across
// games, what each failure answers, and the slots.
#include <doctest.h>

#include "../TestManualJobs.h"
#include "../TestTempDir.h"
#include "Application/Services/IJobSystem.h"
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

    // A class whose upgrade has a bug: it asks the JSON for a key the old file does not have.
    class TestThrowingSave final : public SaveGame
    {
        OPAAX_OBJECT(TestThrowingSave, SaveGame)

    public:
        Int32 GetSaveVersion() const override { return 2; }

        void UpgradeProperties(Int32 /*InFileVersion*/, nlohmann::json& InOutProperties) const override
        {
            InOutProperties["Renamed"] = InOutProperties.at("NotThere");
        }
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
            Classes.Register<TestThrowingSave>();
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
            Game->Init(GameInstanceContext{ Worlds, Resources, IPaths::Null(), Events, Input, Config,
                                            InJobs, Storage });
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

TEST_CASE("SaveGameSubsystem: a class whose upgrade throws answers Corrupt instead of ending the program")
{
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame();

    REQUIRE(lFixture.Storage.Write("Old", R"({ "Class": "TestThrowingSave", "ClassVersion": 1 })") == ESaveResult::Ok);

    TStrongObjectPtr<TestThrowingSave> lLoaded;
    CHECK(lSaves.Load("Old", lLoaded) == ESaveResult::Corrupt);
    CHECK_FALSE(lLoaded.IsValid());

    ESaveResult lAnswer = ESaveResult::Ok;
    lSaves.LoadAsync<TestThrowingSave>("Old", [&lAnswer](const ESaveResult InResult, TStrongObjectPtr<TestThrowingSave>)
    {
        lAnswer = InResult;
    });
    lSaves.Flush();
    CHECK(lAnswer == ESaveResult::Corrupt);
}

// =============================================================================
// In the background
// =============================================================================

TEST_CASE("SaveGameSubsystem: SaveAsync writes on a worker, and answers at the next Update")
{
    TestManualJobs     lJobs;
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame(lJobs);

    const TStrongObjectPtr<TestRunSave> lSave = NewObject<TestRunSave>();
    lSave->Coins = 7;

    TDynArray<ESaveResult> lAnswers;
    lSaves.SaveAsync("Main", *lSave, [&lAnswers](const ESaveResult InResult) { lAnswers.push_back(InResult); });

    // Asked, not run yet: nothing on the disk, no answer.
    CHECK(lJobs.HasJobs());
    CHECK(lSaves.IsBusy());
    CHECK_FALSE(lFixture.Storage.Exists("Main"));
    CHECK(lAnswers.empty());

    // The worker writes; the answer waits for the main thread.
    lJobs.RunAll();
    CHECK(lFixture.Storage.Exists("Main"));
    CHECK(lAnswers.empty());
    CHECK(lSaves.IsBusy());

    lFixture.Game->Update(0.016);
    REQUIRE(lAnswers.size() == 1);
    CHECK(lAnswers[0] == ESaveResult::Ok);
    CHECK_FALSE(lSaves.IsBusy());
}

TEST_CASE("SaveGameSubsystem: SaveAsync saves the object as it was when asked")
{
    TestManualJobs     lJobs;
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame(lJobs);

    const TStrongObjectPtr<TestRunSave> lSave = NewObject<TestRunSave>();
    lSave->Coins = 1;
    lSaves.SaveAsync("Main", *lSave);

    // The game goes on before the worker runs.
    lSave->Coins = 2;
    lJobs.RunAll();
    lFixture.Game->Update(0.016);

    TStrongObjectPtr<TestRunSave> lLoaded;
    REQUIRE(lSaves.Load("Main", lLoaded) == ESaveResult::Ok);
    CHECK(lLoaded->Coins == 1);
}

TEST_CASE("SaveGameSubsystem: LoadAsync hands over the save, or why there is none")
{
    // The null job system runs each job at once: the answers still wait for the next Update.
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame();

    REQUIRE(lFixture.Storage.Write("Main", R"({ "Class": "TestRunSave", "Properties": { "Coins": 9 } })")
            == ESaveResult::Ok);
    REQUIRE(lFixture.Storage.Write("Options", R"({ "Class": "TestOptionsSave" })") == ESaveResult::Ok);

    struct Answer
    {
        ESaveResult Result;
        Int32       Coins;
    };
    TDynArray<Answer> lAnswers;
    const auto lRecord = [&lAnswers](const ESaveResult InResult, const TStrongObjectPtr<TestRunSave> InSave)
    {
        lAnswers.push_back({ InResult, InSave.IsValid() ? InSave->Coins : -1 });
    };

    lSaves.LoadAsync<TestRunSave>("Main", lRecord);
    lSaves.LoadAsync<TestRunSave>("Missing", lRecord);
    lSaves.LoadAsync<TestRunSave>("Options", lRecord);
    CHECK(lAnswers.empty());

    lFixture.Game->Update(0.016);

    REQUIRE(lAnswers.size() == 3);
    CHECK(lAnswers[0].Result == ESaveResult::Ok);
    CHECK(lAnswers[0].Coins  == 9);
    CHECK(lAnswers[1].Result == ESaveResult::Missing);
    CHECK(lAnswers[1].Coins  == -1);
    CHECK(lAnswers[2].Result == ESaveResult::WrongClass);
    CHECK(lAnswers[2].Coins  == -1);
}

TEST_CASE("SaveGameSubsystem: background saves and loads run in the order asked")
{
    JobSystem          lJobs;
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame(lJobs);

    const TStrongObjectPtr<TestRunSave> lSave = NewObject<TestRunSave>();
    TDynArray<OpaaxString>              lOrder;
    Int32                               lLoadedCoins = -1;

    lSave->Coins = 1;
    lSaves.SaveAsync("Main", *lSave, [&lOrder](const ESaveResult InResult)
    {
        lOrder.push_back(OpaaxString::Format("save 1: {}", ToString(InResult)));
    });

    lSave->Coins = 2;
    lSaves.SaveAsync("Main", *lSave, [&lOrder](ESaveResult) { lOrder.push_back(OpaaxString("save 2")); });

    lSaves.LoadAsync<TestRunSave>("Main", [&](const ESaveResult InResult, const TStrongObjectPtr<TestRunSave> InLoaded)
    {
        lOrder.push_back(OpaaxString("load"));
        lLoadedCoins = (InResult == ESaveResult::Ok) ? InLoaded->Coins : -2;
    });

    lSave->Coins = 3;
    lSaves.SaveAsync("Main", *lSave, [&lOrder](ESaveResult) { lOrder.push_back(OpaaxString("save 3")); });

    lSaves.Flush();
    CHECK_FALSE(lSaves.IsBusy());

    REQUIRE(lOrder.size() == 4);
    CHECK(lOrder[0] == "save 1: Ok");
    CHECK(lOrder[1] == "save 2");
    CHECK(lOrder[2] == "load");
    CHECK(lOrder[3] == "save 3");

    // The load read the save asked just before it; the last save is on the disk.
    CHECK(lLoadedCoins == 2);

    TStrongObjectPtr<TestRunSave> lLoaded;
    REQUIRE(lSaves.Load("Main", lLoaded) == ESaveResult::Ok);
    CHECK(lLoaded->Coins == 3);
}

TEST_CASE("SaveGameSubsystem: a call that is not async waits for the background ones asked before it")
{
    JobSystem          lJobs;
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame(lJobs);

    const TStrongObjectPtr<TestRunSave> lSave = NewObject<TestRunSave>();
    lSave->Coins = 5;
    lSaves.SaveAsync("Main", *lSave);

    CHECK(lSaves.Exists("Main"));

    TStrongObjectPtr<TestRunSave> lLoaded;
    REQUIRE(lSaves.Load("Main", lLoaded) == ESaveResult::Ok);
    CHECK(lLoaded->Coins == 5);

    // Written, but reported at the next Update.
    CHECK(lSaves.IsBusy());
    lFixture.Game->Update(0.016);
    CHECK_FALSE(lSaves.IsBusy());
}

TEST_CASE("SaveGameSubsystem: when the game ends, what was asked is written")
{
    JobSystem          lJobs;
    SaveFixture        lFixture;
    SaveGameSubsystem& lSaves = lFixture.StartGame(lJobs);

    const TStrongObjectPtr<TestRunSave> lSave = NewObject<TestRunSave>();
    Int32                               lAnswered = 0;

    SUBCASE("asked before the game ends: its callback runs at TearDown, while the game is whole")
    {
        lSaves.SaveAsync("Main", *lSave, [&lAnswered](ESaveResult) { ++lAnswered; });
        lFixture.EndGame();

        CHECK(lAnswered == 1);
        CHECK(lFixture.Storage.Exists("Main"));
    }

    SUBCASE("asked after TearDown: written at Shutdown, its callback not run")
    {
        GameInstanceSubsystemMgr& lSubsystems = lFixture.Game->GetSubsystems();
        lSubsystems.TearDownAll();

        lSaves.SaveAsync("Late", *lSave, [&lAnswered](ESaveResult) { ++lAnswered; });
        lSubsystems.ShutdownAll();

        CHECK(lAnswered == 0);
        CHECK(lFixture.Storage.Exists("Late"));
    }
}
