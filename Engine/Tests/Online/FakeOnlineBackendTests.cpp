// Suite: FakeOnlineBackend (Online/Fake/FakeOnlineBackend.h) through OnlineServices, as a game uses
// them: leaderboards and their ranges, ghosts, names the store learns late, each store's profile, the
// overlay, achievements.
#include <doctest.h>

#include "Application/Services/IOnlineServices.h"
#include "Online/Fake/FakeOnlineBackend.h"

using namespace Opaax;

namespace
{
    const LeaderboardDefinition LEVEL1 = { "Level1", ELeaderboardSort::Ascending, ELeaderboardDisplay::TimeMilliseconds };

    /** Services over a fake store of InProfile; the store stays reachable through OutStore. */
    TUniquePtr<OnlineServices> MakeOnline(const EFakeStoreProfile InProfile, FakeOnlineBackend*& OutStore)
    {
        TUniquePtr<FakeOnlineBackend> lStore = MakeUnique<FakeOnlineBackend>(InProfile);
        OutStore = lStore.get();
        TUniquePtr<OnlineServices> lOnline = MakeUnique<OnlineServices>(Move(lStore), "test");
        lOnline->SetLeaderboards({ LEVEL1 });
        return lOnline;
    }

    TDynArray<LeaderboardEntry> Download(OnlineServices& InOnline, const ELeaderboardRange InRange, const Int32 InFirst,
                                         const Int32 InLast, EOnlineResult* OutResult = nullptr)
    {
        TDynArray<LeaderboardEntry> lEntries;
        EOnlineResult lResult = EOnlineResult::Failed;
        InOnline.DownloadEntries("Level1", InRange, InFirst, InLast,
                                 [&](const EOnlineResult InResult, const TDynArray<LeaderboardEntry>& InEntries)
                                 {
                                     lResult  = InResult;
                                     lEntries = InEntries;
                                 });
        InOnline.Pump();
        if (OutResult != nullptr)
        {
            *OutResult = lResult;
        }
        return lEntries;
    }

    TDynArray<OpaaxString> Names(const TDynArray<LeaderboardEntry>& InEntries)
    {
        TDynArray<OpaaxString> lNames;
        for (const LeaderboardEntry& lEntry : InEntries)
        {
            lNames.push_back(lEntry.Player.Name);
        }
        return lNames;
    }

    // A game's results screen, written against IOnlineServices only: it uploads a run with its ghost,
    // then shows the lines around the player.
    struct ResultsScreen
    {
        bool                   bUploaded = false;
        bool                   bNewBest  = false;
        TDynArray<OpaaxString> Lines;

        void Show(IOnlineServices& InOnline, const Int32 InTimeMs, const TDynArray<Uint8>& InGhost)
        {
            const TDynArray<Uint8> lGhost = InOnline.GetCapabilities().bLeaderboardFiles ? InGhost : TDynArray<Uint8>{};
            InOnline.UploadScore("Level1", InTimeMs, {}, lGhost,
                [this, &InOnline](const EOnlineResult InResult, const ScoreUploadResult& InUpload)
                {
                    bUploaded = InResult == EOnlineResult::Ok;
                    bNewBest  = InUpload.bScoreChanged;
                    InOnline.DownloadEntries("Level1", ELeaderboardRange::AroundPlayer, -1, 1,
                        [this](EOnlineResult, const TDynArray<LeaderboardEntry>& InEntries)
                        {
                            for (const LeaderboardEntry& lEntry : InEntries)
                            {
                                Lines.push_back(OpaaxString::Format("{}. {} {}", lEntry.Rank, lEntry.Player.Name.CStr(), lEntry.Score));
                            }
                        });
                });
        }
    };
}

TEST_CASE("FakeOnlineBackend: profiles are read by name and give each store's features")
{
    EFakeStoreProfile lProfile = EFakeStoreProfile::Full;
    CHECK(ParseFakeStoreProfile("Epic", lProfile));
    CHECK(lProfile == EFakeStoreProfile::Epic);
    CHECK(ParseFakeStoreProfile("gog", lProfile));
    CHECK(lProfile == EFakeStoreProfile::Gog);
    CHECK_FALSE(ParseFakeStoreProfile("xbox", lProfile));
    CHECK(lProfile == EFakeStoreProfile::Gog);

    const OnlineCapabilities lSteam = GetFakeStoreCapabilities(EFakeStoreProfile::Steam);
    CHECK(lSteam.bLeaderboardsMadeAtRunTime);
    CHECK(lSteam.bFriendsRange);
    CHECK(lSteam.bLeaderboardFiles);

    const OnlineCapabilities lEpic = GetFakeStoreCapabilities(EFakeStoreProfile::Epic);
    CHECK(lEpic.bLeaderboards);
    CHECK_FALSE(lEpic.bLeaderboardsMadeAtRunTime);
    CHECK_FALSE(lEpic.bFriendsRange);
    CHECK_FALSE(lEpic.bLeaderboardFiles);
    CHECK(lEpic.MaxDetails == 0);

    const OnlineCapabilities lGog = GetFakeStoreCapabilities(EFakeStoreProfile::Gog);
    CHECK(lGog.bLeaderboardFiles);
    CHECK_FALSE(lGog.bFriendsRange);
}

TEST_CASE("FakeOnlineBackend: a board made on first use keeps each player's best, ranked")
{
    FakeOnlineBackend* lStore = nullptr;
    TUniquePtr<OnlineServices> lOnline = MakeOnline(EFakeStoreProfile::Full, lStore);

    CHECK(lOnline->GetBackend() == EOnlineBackend::Fake);
    CHECK(lOnline->GetLocalPlayer().Name == "Player");

    // The board does not exist until the first upload makes it.
    CHECK(lStore->FindEntries("Level1") == nullptr);

    ScoreUploadResult lUpload;
    const auto lRecord = [&lUpload](EOnlineResult, const ScoreUploadResult& InUpload) { lUpload = InUpload; };
    lOnline->UploadScore("Level1", 1500, {}, {}, lRecord);
    lOnline->Pump();
    CHECK(lUpload.bScoreChanged);
    CHECK(lUpload.Rank == 1);
    CHECK(lUpload.PreviousRank == 0);

    const OnlinePlayerId lAda = lStore->AddPlayer("Ada", true);
    const OnlinePlayerId lBob = lStore->AddPlayer("Bob", false);
    REQUIRE(lStore->SetScore("Level1", lAda, 1000));
    REQUIRE(lStore->SetScore("Level1", lBob, 1200));
    CHECK_FALSE(lStore->SetScore("Nope", lBob, 1));

    // The player is third; a better time makes them second.
    lOnline->UploadScore("Level1", 1100, {}, {}, lRecord);
    lOnline->Pump();
    CHECK(lUpload.bScoreChanged);
    CHECK(lUpload.PreviousRank == 3);
    CHECK(lUpload.Rank == 2);

    // A worse one changes nothing.
    lOnline->UploadScore("Level1", 1300, {}, {}, lRecord);
    lOnline->Pump();
    CHECK_FALSE(lUpload.bScoreChanged);
    CHECK(lUpload.Rank == 2);

    CHECK(Names(Download(*lOnline, ELeaderboardRange::World, 1, 10)) == TDynArray<OpaaxString>{ "Ada", "Player", "Bob" });
    CHECK(Names(Download(*lOnline, ELeaderboardRange::World, 1, 1)) == TDynArray<OpaaxString>{ "Ada" });
    CHECK(Names(Download(*lOnline, ELeaderboardRange::AroundPlayer, 0, 1)) == TDynArray<OpaaxString>{ "Player", "Bob" });
    CHECK(Names(Download(*lOnline, ELeaderboardRange::Friends, 0, 0)) == TDynArray<OpaaxString>{ "Ada", "Player" });

    const TDynArray<LeaderboardEntry> lAll = Download(*lOnline, ELeaderboardRange::World, 1, 10);
    REQUIRE(lAll.size() == 3);
    CHECK(lAll[1].Score == 1100);
    CHECK(lAll[1].Rank == 2);
}

TEST_CASE("FakeOnlineBackend: a ghost stays with the best run, and answers wait for the store's latency")
{
    FakeOnlineBackend* lStore = nullptr;
    TUniquePtr<OnlineServices> lOnline = MakeOnline(EFakeStoreProfile::Steam, lStore);
    lStore->SetLatency(2);

    EOnlineResult lResult = EOnlineResult::Failed;
    bool bAnswered = false;
    const auto lRecord = [&](const EOnlineResult InResult, const ScoreUploadResult&)
    {
        lResult   = InResult;
        bAnswered = true;
    };

    // Find, upload and attach each take two pumps.
    lOnline->UploadScore("Level1", 1000, {}, { 1, 2, 3 }, lRecord);
    Uint32 lPumps = 0;
    while (!bAnswered && lPumps < 20)
    {
        lOnline->Pump();
        ++lPumps;
    }
    CHECK(bAnswered);
    CHECK(lResult == EOnlineResult::Ok);
    CHECK(lPumps >= 6);

    lStore->SetLatency(0);
    lOnline->UploadScore("Level1", 1400, {}, { 9 }, lRecord);
    lOnline->Pump();

    const TDynArray<LeaderboardEntry> lEntries = Download(*lOnline, ELeaderboardRange::World, 1, 1);
    REQUIRE(lEntries.size() == 1);
    CHECK(lEntries[0].Score == 1000);
    REQUIRE(lEntries[0].File.IsValid());

    TDynArray<Uint8> lGhost;
    lOnline->DownloadFile(lEntries[0].File, [&lGhost](EOnlineResult, const TDynArray<Uint8>& InBytes) { lGhost = InBytes; });
    lOnline->Pump();
    CHECK(lGhost == TDynArray<Uint8>{ 1, 2, 3 });

    // A file past the store's size is refused; the score itself was kept.
    lOnline->UploadScore("Level1", 900, {}, TDynArray<Uint8>(2 * 1024 * 1024, 7), lRecord);
    lOnline->Pump();
    CHECK(lResult == EOnlineResult::QuotaExceeded);
    CHECK(lStore->FindEntries("Level1")->front().Score == 900);
}

TEST_CASE("FakeOnlineBackend: as Epic, boards come from the portal, files are unsupported, details dropped")
{
    FakeOnlineBackend* lStore = nullptr;
    TUniquePtr<OnlineServices> lOnline = MakeOnline(EFakeStoreProfile::Epic, lStore);

    EOnlineResult lResult = EOnlineResult::Ok;
    const auto lRecord = [&lResult](const EOnlineResult InResult, const ScoreUploadResult&) { lResult = InResult; };

    // Not in the portal: not found, and not made.
    lOnline->UploadScore("Level1", 1000, {}, {}, lRecord);
    lOnline->Pump();
    CHECK(lResult == EOnlineResult::NotFound);
    CHECK(lStore->FindEntries("Level1") == nullptr);

    lStore->DefineLeaderboard(LEVEL1);
    lOnline->UploadScore("Level1", 1000, {}, { 1 }, lRecord);
    lOnline->Pump();
    CHECK(lResult == EOnlineResult::Unsupported);

    lOnline->UploadScore("Level1", 1000, { 4, 5 }, {}, lRecord);
    lOnline->Pump();
    CHECK(lResult == EOnlineResult::Ok);
    REQUIRE(lStore->FindEntries("Level1")->size() == 1);
    CHECK(lStore->FindEntries("Level1")->front().Details.empty());

    // No friends range in the store: the services build it from the friends list.
    const OnlinePlayerId lAda = lStore->AddPlayer("Ada", true);
    const OnlinePlayerId lBob = lStore->AddPlayer("Bob", false);
    lStore->SetScore("Level1", lAda, 1200);
    lStore->SetScore("Level1", lBob, 800);

    const TDynArray<LeaderboardEntry> lFriends = Download(*lOnline, ELeaderboardRange::Friends, 0, 0);
    CHECK(Names(lFriends) == TDynArray<OpaaxString>{ "Player", "Ada" });
    REQUIRE(lFriends.size() == 2);
    CHECK(lFriends[0].Rank == 2);
    CHECK(lFriends[1].Rank == 3);
}

TEST_CASE("FakeOnlineBackend: names the store learns late are waited for, up to the timeout")
{
    FakeOnlineBackend* lStore = nullptr;
    TUniquePtr<OnlineServices> lOnline = MakeOnline(EFakeStoreProfile::Full, lStore);

    double lNow = 100.0;
    lOnline->SetClock([&lNow]() { return lNow; });

    lOnline->UploadScore("Level1", 1000, {}, {}, nullptr);
    lOnline->Pump();
    const OnlinePlayerId lStranger = lStore->AddPlayer("Stranger", false, false);
    lStore->SetScore("Level1", lStranger, 900);

    // The store answers the names: the download comes with them.
    lStore->SetLatency(1);
    TDynArray<LeaderboardEntry> lEntries;
    lOnline->DownloadEntries("Level1", ELeaderboardRange::World, 1, 10,
                             [&lEntries](EOnlineResult, const TDynArray<LeaderboardEntry>& InEntries) { lEntries = InEntries; });
    for (Uint32 lPump = 0; lPump < 4; ++lPump)
    {
        lOnline->Pump();
    }
    CHECK(Names(lEntries) == TDynArray<OpaaxString>{ "Stranger", "Player" });

    // A store that never answers: the download waits 3 s, then comes with the name empty.
    lStore->SetLatency(0);
    lStore->SetNamesAnswer(false);
    const OnlinePlayerId lSilent = lStore->AddPlayer("Silent", false, false);
    lStore->SetScore("Level1", lSilent, 800);

    bool bAnswered = false;
    lEntries.clear();
    lOnline->DownloadEntries("Level1", ELeaderboardRange::World, 1, 1,
                             [&](const EOnlineResult InResult, const TDynArray<LeaderboardEntry>& InEntries)
                             {
                                 CHECK(InResult == EOnlineResult::Ok);
                                 lEntries  = InEntries;
                                 bAnswered = true;
                             });
    lOnline->Pump();
    lNow += 2.9;
    lOnline->Pump();
    CHECK_FALSE(bAnswered);
    CHECK(lOnline->GetPendingCount() == 1);

    lNow += 0.2;
    lOnline->Pump();
    lOnline->Pump();
    CHECK(bAnswered);
    REQUIRE(lEntries.size() == 1);
    CHECK(lEntries[0].Player.Id == lSilent);
    CHECK(lEntries[0].Player.Name.IsEmpty());
}

TEST_CASE("FakeOnlineBackend: the overlay and achievements")
{
    FakeOnlineBackend* lStore = nullptr;
    TUniquePtr<OnlineServices> lOnline = MakeOnline(EFakeStoreProfile::Full, lStore);

    TDynArray<bool> lOverlay;
    lOnline->OnOverlayChanged().Add([&lOverlay](const bool bInActive) { lOverlay.push_back(bInActive); });
    lStore->SetOverlayActive(true);
    lStore->SetOverlayActive(false);
    CHECK(lOnline->OpenOverlay(EOverlayPage::Leaderboards));
    CHECK(lOnline->IsOverlayActive());
    CHECK(lOverlay == TDynArray<bool>{ true, false, true });

    EOnlineResult lResult = EOnlineResult::Failed;
    const auto lRecord = [&lResult](const EOnlineResult InResult) { lResult = InResult; };

    lOnline->UnlockAchievement("FirstRun", lRecord);
    lOnline->Pump();
    CHECK(lResult == EOnlineResult::Ok);
    CHECK(lStore->IsAchievementUnlocked("FirstRun"));

    TDynArray<OnlineAchievement> lList;
    lOnline->GetAchievements([&lList](EOnlineResult, const TDynArray<OnlineAchievement>& InList) { lList = InList; });
    lOnline->Pump();
    REQUIRE(lList.size() == 1);
    CHECK(lList[0].Name == "FirstRun");
    CHECK(lList[0].bUnlocked);

    lOnline->ClearAchievement("FirstRun", lRecord);
    lOnline->Pump();
    CHECK(lResult == EOnlineResult::Ok);
    CHECK_FALSE(lStore->IsAchievementUnlocked("FirstRun"));

    lOnline->ClearAchievement("Never", lRecord);
    lOnline->Pump();
    CHECK(lResult == EOnlineResult::NotFound);
}

TEST_CASE("FakeOnlineBackend: a game's results screen, tested without a store")
{
    for (const EFakeStoreProfile lProfile : { EFakeStoreProfile::Steam, EFakeStoreProfile::Gog, EFakeStoreProfile::Epic })
    {
        CAPTURE(ToString(lProfile));
        FakeOnlineBackend* lStore = nullptr;
        TUniquePtr<OnlineServices> lOnline = MakeOnline(lProfile, lStore);
        lStore->DefineLeaderboard(LEVEL1);
        lStore->SetScore("Level1", lStore->AddPlayer("Ada", true), 1000);
        lStore->SetScore("Level1", lStore->AddPlayer("Bob", false), 1300);

        ResultsScreen lScreen;
        lScreen.Show(*lOnline, 1200, { 1, 2 });
        lOnline->Pump();
        lOnline->Pump();

        CHECK(lScreen.bUploaded);
        CHECK(lScreen.bNewBest);
        CHECK(lScreen.Lines == TDynArray<OpaaxString>{ "1. Ada 1000", "2. Player 1200", "3. Bob 1300" });
    }
}
