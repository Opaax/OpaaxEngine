// Suite: OnlineServices (Application/Services/IOnlineServices.h), the store behind one set of calls.
// The null backend answers Unavailable; a store without a feature answers Unsupported; answers come
// at the next Pump, never inside the call. A small store written here stands for a real one.
#include <doctest.h>

#include "Application/Services/IOnlineServices.h"
#include "Online/Null/NullOnlineBackend.h"

using namespace Opaax;

namespace
{
    // Stores stopped: a store is destroyed when the services stop it.
    Uint32 g_Shutdowns = 0;

    // A store with leaderboards (one score per player, kept at its best, smallest first) and, when
    // asked, files. It answers inside each call: the services must still wait for Pump.
    class TestStore final : public IOnlineBackend, public IOnlineLeaderboards, public IOnlineFiles, public IOnlineOverlay
    {
    public:
        explicit TestStore(const bool bInFiles) : m_bFiles(bInFiles) {}

        EOnlineBackend     GetType() const noexcept override { return EOnlineBackend::Fake; }
        OnlineCapabilities GetCapabilities() const override
        {
            OnlineCapabilities lCapabilities;
            lCapabilities.bLeaderboards     = true;
            lCapabilities.bLeaderboardFiles = m_bFiles;
            lCapabilities.bOverlay          = true;
            return lCapabilities;
        }

        IOnlineLeaderboards* GetLeaderboards() override { return this; }
        IOnlineFiles*        GetFiles() override        { return m_bFiles ? this : nullptr; }
        IOnlineOverlay*      GetOverlay() override      { return this; }

        void Shutdown() override { ++g_Shutdowns; }

        void FindLeaderboard(const LeaderboardDefinition& InDefinition, FOnLeaderboardFound InDone) override
        {
            ++Finds;
            InDone(EOnlineResult::Ok, OnlineLeaderboardHandle{ InDefinition.Name });
        }

        void UploadScore(const OnlineLeaderboardHandle&, const Int32 InScore, const TDynArray<Int32>&,
                         FOnScoreUploaded InDone) override
        {
            ScoreUploadResult lResult;
            lResult.bScoreChanged = !bHasScore || InScore < Best;
            if (lResult.bScoreChanged)
            {
                Best      = InScore;
                bHasScore = true;
            }
            lResult.Rank = 1;
            InDone(EOnlineResult::Ok, lResult);
        }

        void DownloadEntries(const OnlineLeaderboardHandle&, ELeaderboardRange, Int32, Int32, FOnEntries InDone) override
        {
            TDynArray<LeaderboardEntry> lEntries;
            if (bHasScore)
            {
                LeaderboardEntry lEntry;
                lEntry.Rank  = 1;
                lEntry.Score = Best;
                lEntry.File  = AttachedFile;
                lEntries.push_back(lEntry);
            }
            InDone(EOnlineResult::Ok, lEntries);
        }

        void AttachFile(const OnlineLeaderboardHandle&, const TDynArray<Uint8>& InBytes, FOnOnlineDone InDone) override
        {
            File         = InBytes;
            AttachedFile = OnlineFileId{ "file-1" };
            InDone(EOnlineResult::Ok);
        }

        void DownloadFile(const OnlineFileId&, FOnFile InDone) override { InDone(EOnlineResult::Ok, File); }

        bool IsActive() const override { return false; }
        bool Open(EOverlayPage, const OnlinePlayerId&) override { return true; }

        void SetOverlay(const bool bInActive) const { NotifyOverlayChanged(bInActive); }

        bool             bHasScore = false;
        Int32            Best      = 0;
        TDynArray<Uint8> File;
        OnlineFileId     AttachedFile;
        Uint32           Finds     = 0;

    private:
        bool m_bFiles = false;
    };

    const TDynArray<LeaderboardDefinition> BOARDS = { LeaderboardDefinition{ "Level1", ELeaderboardSort::Ascending,
                                                                             ELeaderboardDisplay::TimeMilliseconds } };
}

TEST_CASE("OnlineServices: with no store, every request answers Unavailable once, at the next Pump")
{
    OnlineServices lOnline(MakeUnique<NullOnlineBackend>(), "no store built in");
    lOnline.SetLeaderboards(BOARDS);

    CHECK_FALSE(lOnline.IsAvailable());
    CHECK(lOnline.GetBackend() == EOnlineBackend::None);
    CHECK(lOnline.GetBackendReason() == "no store built in");
    CHECK_FALSE(lOnline.GetLocalPlayer().Id.IsValid());
    CHECK_FALSE(lOnline.GetCapabilities().bLeaderboards);
    CHECK_FALSE(lOnline.OpenOverlay(EOverlayPage::Friends));
    CHECK_FALSE(lOnline.IsOverlayActive());

    TDynArray<EOnlineResult> lAnswers;
    const auto lRecord      = [&lAnswers](const EOnlineResult InResult) { lAnswers.push_back(InResult); };
    const auto lRecordValue = [&lAnswers](const EOnlineResult InResult, const auto&) { lAnswers.push_back(InResult); };

    lOnline.RequestPlayerNames({ OnlinePlayerId{ EOnlineBackend::Steam, "1" } }, lRecord);
    lOnline.UploadScore("Level1", 1000, {}, {}, lRecordValue);
    lOnline.UploadScore("Level1", 1000, {}, { 1, 2, 3 }, lRecordValue);
    lOnline.DownloadEntries("Level1", ELeaderboardRange::World, 1, 10, lRecordValue);
    lOnline.DownloadFile(OnlineFileId{ "x" }, lRecordValue);
    lOnline.UnlockAchievement("A", lRecord);
    lOnline.IndicateAchievementProgress("A", 1, 2, lRecord);
    lOnline.GetAchievements(lRecordValue);
    lOnline.ClearAchievement("A", lRecord);

    // Nothing inside the calls.
    CHECK(lAnswers.empty());
    CHECK(lOnline.GetPendingCount() == 9);

    lOnline.Pump();
    REQUIRE(lAnswers.size() == 9);
    for (const EOnlineResult lAnswer : lAnswers)
    {
        CHECK(lAnswer == EOnlineResult::Unavailable);
    }
    CHECK(lOnline.GetPendingCount() == 0);

    // Once only.
    lOnline.Pump();
    CHECK(lAnswers.size() == 9);
}

TEST_CASE("OnlineServices: a cancelled request never answers")
{
    OnlineServices lOnline(MakeUnique<NullOnlineBackend>(), "test");

    bool bAnswered = false;
    const OnlineRequestId lRequest = lOnline.UnlockAchievement("A", [&bAnswered](EOnlineResult) { bAnswered = true; });
    CHECK(lRequest != INVALID_ONLINE_REQUEST);

    lOnline.Cancel(lRequest);
    CHECK(lOnline.GetPendingCount() == 0);
    lOnline.Pump();
    CHECK_FALSE(bAnswered);

    // A request answered already: nothing to cancel.
    bool bSecond = false;
    const OnlineRequestId lSecond = lOnline.UnlockAchievement("A", [&bSecond](EOnlineResult) { bSecond = true; });
    lOnline.Pump();
    CHECK(bSecond);
    lOnline.Cancel(lSecond);
}

TEST_CASE("OnlineServices: a store without a feature answers Unsupported, a board not listed NotFound")
{
    OnlineServices lOnline(MakeUnique<TestStore>(false), "test");
    lOnline.SetLeaderboards(BOARDS);
    CHECK(lOnline.IsAvailable());

    EOnlineResult lWithFile = EOnlineResult::Ok;
    EOnlineResult lPlain    = EOnlineResult::Failed;
    EOnlineResult lUnknown  = EOnlineResult::Ok;
    EOnlineResult lNames    = EOnlineResult::Ok;
    EOnlineResult lAchieved = EOnlineResult::Ok;

    lOnline.UploadScore("Level1", 900, {}, { 7 }, [&](const EOnlineResult InResult, const ScoreUploadResult&) { lWithFile = InResult; });
    lOnline.UploadScore("Level1", 900, {}, {}, [&](const EOnlineResult InResult, const ScoreUploadResult&) { lPlain = InResult; });
    lOnline.DownloadEntries("Nope", ELeaderboardRange::World, 1, 10,
                            [&](const EOnlineResult InResult, const TDynArray<LeaderboardEntry>&) { lUnknown = InResult; });
    lOnline.RequestPlayerNames({}, [&](const EOnlineResult InResult) { lNames = InResult; });
    lOnline.UnlockAchievement("A", [&](const EOnlineResult InResult) { lAchieved = InResult; });

    // The store answered inside the calls; the game still hears nothing before Pump.
    CHECK(lPlain == EOnlineResult::Failed);

    lOnline.Pump();
    CHECK(lWithFile == EOnlineResult::Unsupported);
    CHECK(lPlain == EOnlineResult::Ok);
    CHECK(lUnknown == EOnlineResult::NotFound);
    CHECK(lNames == EOnlineResult::Unsupported);
    CHECK(lAchieved == EOnlineResult::Unsupported);
}

TEST_CASE("OnlineServices: a file is attached to a kept score only")
{
    TUniquePtr<TestStore> lStoreOwner = MakeUnique<TestStore>(true);
    TestStore&            lStore      = *lStoreOwner;
    OnlineServices        lOnline(Move(lStoreOwner), "test");
    lOnline.SetLeaderboards(BOARDS);

    ScoreUploadResult lUpload;
    EOnlineResult     lResult = EOnlineResult::Failed;
    const auto lRecord = [&](const EOnlineResult InResult, const ScoreUploadResult& InUpload)
    {
        lResult = InResult;
        lUpload = InUpload;
    };

    lOnline.UploadScore("Level1", 1000, {}, { 1, 1 }, lRecord);
    lOnline.Pump();
    CHECK(lResult == EOnlineResult::Ok);
    CHECK(lUpload.bScoreChanged);
    CHECK(lStore.File == TDynArray<Uint8>{ 1, 1 });

    // A worse run: the best entry keeps its file.
    lOnline.UploadScore("Level1", 1200, {}, { 2, 2 }, lRecord);
    lOnline.Pump();
    CHECK(lResult == EOnlineResult::Ok);
    CHECK_FALSE(lUpload.bScoreChanged);
    CHECK(lStore.File == TDynArray<Uint8>{ 1, 1 });

    // A better one replaces it.
    lOnline.UploadScore("Level1", 800, {}, { 3 }, lRecord);
    lOnline.Pump();
    CHECK(lUpload.bScoreChanged);
    CHECK(lStore.File == TDynArray<Uint8>{ 3 });

    // The board was found once, then kept.
    CHECK(lStore.Finds == 1);

    // The entry carries its file, which downloads.
    OnlineFileId lFile;
    lOnline.DownloadEntries("Level1", ELeaderboardRange::World, 1, 10,
                            [&](EOnlineResult, const TDynArray<LeaderboardEntry>& InEntries)
                            {
                                REQUIRE(InEntries.size() == 1);
                                CHECK(InEntries[0].Score == 800);
                                lFile = InEntries[0].File;
                            });
    lOnline.Pump();
    REQUIRE(lFile.IsValid());

    TDynArray<Uint8> lBytes;
    lOnline.DownloadFile(lFile, [&](EOnlineResult, const TDynArray<Uint8>& InBytes) { lBytes = InBytes; });
    lOnline.Pump();
    CHECK(lBytes == TDynArray<Uint8>{ 3 });
}

TEST_CASE("OnlineServices: the overlay's opening and closing are broadcast once each")
{
    TUniquePtr<TestStore> lStoreOwner = MakeUnique<TestStore>(false);
    const TestStore&      lStore      = *lStoreOwner;
    OnlineServices        lOnline(Move(lStoreOwner), "test");

    TDynArray<bool> lChanges;
    lOnline.OnOverlayChanged().Add([&lChanges](const bool bInActive) { lChanges.push_back(bInActive); });

    lStore.SetOverlay(true);
    CHECK(lOnline.IsOverlayActive());
    lStore.SetOverlay(true);
    lStore.SetOverlay(false);
    CHECK_FALSE(lOnline.IsOverlayActive());

    CHECK(lChanges == TDynArray<bool>{ true, false });
    CHECK(lOnline.OpenOverlay(EOverlayPage::Achievements));
}

TEST_CASE("OnlineServices: after shutdown the store is stopped and requests answer Unavailable")
{
    OnlineServices lOnline(MakeUnique<TestStore>(true), "test");
    lOnline.SetLeaderboards(BOARDS);

    bool bDropped = false;
    lOnline.UploadScore("Level1", 1, {}, {}, [&](EOnlineResult, const ScoreUploadResult&) { bDropped = true; });

    const Uint32 lShutdownsBefore = g_Shutdowns;
    lOnline.OnShutdown();
    CHECK(g_Shutdowns == lShutdownsBefore + 1);
    CHECK(lOnline.GetBackend() == EOnlineBackend::None);
    CHECK(lOnline.GetPendingCount() == 0);

    // What was asked before is dropped.
    lOnline.Pump();
    CHECK_FALSE(bDropped);

    EOnlineResult lAfter = EOnlineResult::Ok;
    lOnline.UnlockAchievement("A", [&](const EOnlineResult InResult) { lAfter = InResult; });
    lOnline.Pump();
    CHECK(lAfter == EOnlineResult::Unavailable);
}

TEST_CASE("OnlineServices: the null object answers nothing")
{
    IOnlineServices& lNull = IOnlineServices::Null();
    CHECK(lNull.IsNull());
    CHECK_FALSE(lNull.IsAvailable());

    bool bAnswered = false;
    CHECK(lNull.UnlockAchievement("A", [&](EOnlineResult) { bAnswered = true; }) == INVALID_ONLINE_REQUEST);
    lNull.Pump();
    CHECK_FALSE(bAnswered);
}
