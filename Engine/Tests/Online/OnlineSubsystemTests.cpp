// Suite: OnlineSubsystem — a game's way to the store, started the way a game starts it (a
// GameInstance with a context) over a fake store: its requests are dropped when the game ends, and it
// passes the overlay on to the game.
#include <doctest.h>

#include "Application/Services/IOnlineServices.h"
#include "Application/Services/IPaths.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Online/Fake/FakeOnlineBackend.h"
#include "Online/OnlineSubsystem.h"
#include "Resources/ResourceManager.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    struct OnlineFixture
    {
        WorldManager     Worlds;
        ResourceManager  Resources;
        EngineEventBus   Events;
        InputManager     Input;
        EngineConfigData Config;

        FakeOnlineBackend* Store = nullptr;
        OnlineServices     Online{ MakeStore(Store), "test" };

        OnlineFixture()
        {
            Online.SetLeaderboards({ LeaderboardDefinition{ "Level1", ELeaderboardSort::Ascending,
                                                            ELeaderboardDisplay::TimeMilliseconds } });
        }

        ~OnlineFixture()
        {
            EndGame();
        }

        static TUniquePtr<IOnlineBackend> MakeStore(FakeOnlineBackend*& OutStore)
        {
            TUniquePtr<FakeOnlineBackend> lStore = MakeUnique<FakeOnlineBackend>(EFakeStoreProfile::Full);
            OutStore = lStore.get();
            return lStore;
        }

        /** A new game, its OnlineSubsystem started the way a game starts it. */
        OnlineSubsystem& StartGame()
        {
            EndGame();

            Game = NewObject<GameInstance>();
            Game->Init(GameInstanceContext{ Worlds, Resources, IPaths::Null(), Events, Input, Config,
                                            IJobSystem::Null(), ISaveStorage::Null(), IUserSettings::Null(), Online });
            Game->GetSubsystems().Add<OnlineSubsystem>();
            Game->GetSubsystems().StartupAll();
            return *Game->GetSubsystems().GetSubsystem<OnlineSubsystem>();
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

TEST_CASE("OnlineSubsystem: a game's requests answer while it runs, and are dropped when it ends")
{
    OnlineFixture lFixture;
    lFixture.Store->SetLatency(3);

    OnlineSubsystem& lOnline = lFixture.StartGame();
    CHECK(lOnline.IsAvailable());
    CHECK(lOnline.GetLocalPlayer().Name == "Player");
    CHECK(lOnline.GetCapabilities().bLeaderboardFiles);

    EOnlineResult lFirst = EOnlineResult::Failed;
    lOnline.UnlockAchievement("A", [&lFirst](const EOnlineResult InResult) { lFirst = InResult; });
    CHECK(lOnline.GetPendingCount() == 1);
    for (Uint32 lPump = 0; lPump < 4; ++lPump)
    {
        lFixture.Online.Pump();
    }
    CHECK(lFirst == EOnlineResult::Ok);
    CHECK(lOnline.GetPendingCount() == 0);

    // Asked, then the game ends before the store answers: the callback never runs.
    bool bLateAnswer = false;
    lOnline.UploadScore("Level1", 1000, {}, {}, [&bLateAnswer](EOnlineResult, const ScoreUploadResult&) { bLateAnswer = true; });
    lOnline.GetAchievements([&bLateAnswer](EOnlineResult, const TDynArray<OnlineAchievement>&) { bLateAnswer = true; });
    CHECK(lFixture.Online.GetPendingCount() == 2);

    lFixture.EndGame();
    CHECK(lFixture.Online.GetPendingCount() == 0);
    for (Uint32 lPump = 0; lPump < 10; ++lPump)
    {
        lFixture.Online.Pump();
    }
    CHECK_FALSE(bLateAnswer);

    // The store itself went on: the score was kept.
    REQUIRE(lFixture.Store->FindEntries("Level1") != nullptr);
}

TEST_CASE("OnlineSubsystem: the overlay reaches the game while it runs, not after")
{
    OnlineFixture lFixture;
    OnlineSubsystem& lOnline = lFixture.StartGame();

    TDynArray<bool> lHeard;
    lOnline.OnOverlayChanged().Add([&lHeard](const bool bInActive) { lHeard.push_back(bInActive); });

    lFixture.Store->SetOverlayActive(true);
    CHECK(lOnline.IsOverlayActive());
    lFixture.Store->SetOverlayActive(false);
    CHECK(lHeard == TDynArray<bool>{ true, false });

    CHECK(lOnline.OpenOverlay(EOverlayPage::Friends));
    CHECK(lHeard.size() == 3);

    // Once the game ended, nothing of it listens.
    lFixture.EndGame();
    lFixture.Store->SetOverlayActive(false);
    CHECK(lHeard.size() == 3);
}
