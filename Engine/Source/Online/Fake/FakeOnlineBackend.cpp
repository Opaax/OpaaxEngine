#include "Online/Fake/FakeOnlineBackend.h"

#include "Core/Algo/Algo.h"
#include "Online/OnlineLeaderboardRules.h"

namespace Opaax
{
    // =========================================================================
    // Profiles
    // =========================================================================
    bool ParseFakeStoreProfile(const OpaaxStringView InText, EFakeStoreProfile& OutProfile)
    {
        const OpaaxString lText = InText.ToString().ToLower();
        for (const EFakeStoreProfile lProfile : { EFakeStoreProfile::Full, EFakeStoreProfile::Steam,
                                                  EFakeStoreProfile::Epic, EFakeStoreProfile::Gog })
        {
            if (lText == ToString(lProfile))
            {
                OutProfile = lProfile;
                return true;
            }
        }
        return false;
    }

    OnlineCapabilities GetFakeStoreCapabilities(const EFakeStoreProfile InProfile)
    {
        // Every profile has the player, leaderboards, achievements and an overlay.
        OnlineCapabilities lCapabilities;
        lCapabilities.bIdentity     = true;
        lCapabilities.bLeaderboards = true;
        lCapabilities.bAchievements = true;
        lCapabilities.bOverlay      = true;

        const bool bFiles = InProfile != EFakeStoreProfile::Epic;
        lCapabilities.bLeaderboardsMadeAtRunTime = InProfile != EFakeStoreProfile::Epic;
        lCapabilities.bFriendsRange              = InProfile == EFakeStoreProfile::Full || InProfile == EFakeStoreProfile::Steam;
        lCapabilities.bLeaderboardFiles          = bFiles;
        lCapabilities.MaxFileBytes               = bFiles ? 1024ull * 1024ull : 0;
        lCapabilities.MaxDetails                 = (InProfile == EFakeStoreProfile::Epic) ? 0 : 64;
        return lCapabilities;
    }

    // =========================================================================
    // FakeOnlineBackend — setup
    // =========================================================================
    FakeOnlineBackend::FakeOnlineBackend(const EFakeStoreProfile InProfile)
        : m_Profile(InProfile)
        , m_Capabilities(GetFakeStoreCapabilities(InProfile))
    {
        AddPlayer("Player", false);
    }

    OnlinePlayerId FakeOnlineBackend::AddPlayer(const OpaaxString& InName, const bool bInFriend, const bool bInNameKnown)
    {
        FakePlayer lPlayer;
        lPlayer.Id         = OnlinePlayerId{ EOnlineBackend::Fake, OpaaxString::Format("fake:{}", m_Players.size() + 1) };
        lPlayer.Name       = InName;
        lPlayer.bFriend    = bInFriend;
        lPlayer.bNameKnown = bInNameKnown;
        m_Players.push_back(lPlayer);
        return lPlayer.Id;
    }

    void FakeOnlineBackend::DefineLeaderboard(const LeaderboardDefinition& InDefinition)
    {
        FakeBoard& lBoard = m_Boards[InDefinition.Name];
        lBoard.Definition = InDefinition;
    }

    bool FakeOnlineBackend::SetScore(const OpaaxString& InBoard, const OnlinePlayerId& InPlayer, const Int32 InScore,
                                     const TDynArray<Int32>& InDetails)
    {
        FakeBoard* lBoard = FindBoard(InBoard);
        if (lBoard == nullptr || FindPlayer(InPlayer) == nullptr)
        {
            return false;
        }

        KeepBest(*lBoard, InPlayer, InScore, InDetails);
        return true;
    }

    void FakeOnlineBackend::SetOverlayActive(const bool bInActive)
    {
        m_bOverlayActive = bInActive;
        NotifyOverlayChanged(bInActive);
    }

    const TDynArray<LeaderboardEntry>* FakeOnlineBackend::FindEntries(const OpaaxString& InBoard) const
    {
        const auto lFound = m_Boards.find(InBoard);
        return (lFound != m_Boards.end()) ? &lFound->second.Entries : nullptr;
    }

    bool FakeOnlineBackend::IsAchievementUnlocked(const OpaaxString& InName) const
    {
        const auto lFound = Algo::FindIf(m_Achievements, [&InName](const OnlineAchievement& InAchievement)
        {
            return InAchievement.Name == InName;
        });
        return lFound != m_Achievements.end() && lFound->bUnlocked;
    }

    // =========================================================================
    // FakeOnlineBackend — internal
    // =========================================================================
    void FakeOnlineBackend::Reply(TFunction<void()> InAnswer)
    {
        if (m_Latency == 0)
        {
            InAnswer();
            return;
        }
        m_Delayed.emplace_back(m_Latency, Move(InAnswer));
    }

    void FakeOnlineBackend::Pump()
    {
        // Answers due now run after the countdown, so one that asks again waits its own latency.
        TDynArray<TFunction<void()>> lDue;
        for (auto lIt = m_Delayed.begin(); lIt != m_Delayed.end();)
        {
            if (--lIt->first == 0)
            {
                lDue.push_back(Move(lIt->second));
                lIt = m_Delayed.erase(lIt);
            }
            else
            {
                ++lIt;
            }
        }

        for (TFunction<void()>& lAnswer : lDue)
        {
            lAnswer();
        }
    }

    const FakeOnlineBackend::FakePlayer* FakeOnlineBackend::FindPlayer(const OnlinePlayerId& InId) const
    {
        const auto lFound = Algo::FindIf(m_Players, [&InId](const FakePlayer& InPlayer) { return InPlayer.Id == InId; });
        return (lFound != m_Players.end()) ? &*lFound : nullptr;
    }

    FakeOnlineBackend::FakeBoard* FakeOnlineBackend::FindBoard(const OpaaxString& InName)
    {
        const auto lFound = m_Boards.find(InName);
        return (lFound != m_Boards.end()) ? &lFound->second : nullptr;
    }

    TDynArray<LeaderboardEntry> FakeOnlineBackend::WithNames(TDynArray<LeaderboardEntry> InEntries) const
    {
        for (LeaderboardEntry& lEntry : InEntries)
        {
            lEntry.Player.Name = GetPlayerName(lEntry.Player.Id);
        }
        return InEntries;
    }

    ScoreUploadResult FakeOnlineBackend::KeepBest(FakeBoard& InOutBoard, const OnlinePlayerId& InPlayer, const Int32 InScore,
                                                  const TDynArray<Int32>& InDetails)
    {
        TDynArray<LeaderboardEntry>& lEntries = InOutBoard.Entries;
        const auto lFound = Algo::FindIf(lEntries, [&InPlayer](const LeaderboardEntry& InEntry) { return InEntry.Player.Id == InPlayer; });

        ScoreUploadResult lResult;
        LeaderboardEntry  lEntry;
        lEntry.Player.Id = InPlayer;

        if (lFound != lEntries.end())
        {
            lResult.PreviousRank = lFound->Rank;
            if (!OnlineLeaderboardRules::IsBetter(InOutBoard.Definition.Sort, InScore, lFound->Score))
            {
                lResult.Rank = lFound->Rank;
                return lResult;
            }

            // The entry keeps its file; the score moves to the end, so a tie it makes ranks after the earlier one.
            lEntry.File = lFound->File;
            lEntries.erase(lFound);
        }

        lEntry.Score   = InScore;
        lEntry.Details = InDetails;
        lEntries.push_back(lEntry);
        OnlineLeaderboardRules::SortAndRank(lEntries, InOutBoard.Definition.Sort);

        lResult.bScoreChanged = true;
        const auto lNow = Algo::FindIf(lEntries, [&InPlayer](const LeaderboardEntry& InEntry) { return InEntry.Player.Id == InPlayer; });
        lResult.Rank = lNow->Rank;
        return lResult;
    }

    // =========================================================================
    // IOnlineIdentity
    // =========================================================================
    OnlinePlayer FakeOnlineBackend::GetLocalPlayer() const
    {
        const FakePlayer& lLocal = m_Players.front();
        return OnlinePlayer{ lLocal.Id, lLocal.bNameKnown ? lLocal.Name : OpaaxString() };
    }

    OpaaxString FakeOnlineBackend::GetPlayerName(const OnlinePlayerId& InId) const
    {
        const FakePlayer* lPlayer = FindPlayer(InId);
        return (lPlayer != nullptr && lPlayer->bNameKnown) ? lPlayer->Name : OpaaxString();
    }

    void FakeOnlineBackend::RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone)
    {
        if (!m_bNamesAnswer)
        {
            return;
        }

        Reply([this, InIds, InDone = Move(InDone)]()
        {
            for (FakePlayer& lPlayer : m_Players)
            {
                if (Algo::Contains(InIds, lPlayer.Id))
                {
                    lPlayer.bNameKnown = true;
                }
            }
            InDone(EOnlineResult::Ok);
        });
    }

    TDynArray<OnlinePlayerId> FakeOnlineBackend::GetFriends() const
    {
        TDynArray<OnlinePlayerId> lFriends;
        for (const FakePlayer& lPlayer : m_Players)
        {
            if (lPlayer.bFriend)
            {
                lFriends.push_back(lPlayer.Id);
            }
        }
        return lFriends;
    }

    // =========================================================================
    // IOnlineLeaderboards
    // =========================================================================
    void FakeOnlineBackend::FindLeaderboard(const LeaderboardDefinition& InDefinition, FOnLeaderboardFound InDone)
    {
        if (FindBoard(InDefinition.Name) == nullptr)
        {
            if (!m_Capabilities.bLeaderboardsMadeAtRunTime)
            {
                Reply([InDone = Move(InDone)]() { InDone(EOnlineResult::NotFound, {}); });
                return;
            }
            DefineLeaderboard(InDefinition);
        }

        Reply([InDone = Move(InDone), lName = InDefinition.Name]() { InDone(EOnlineResult::Ok, OnlineLeaderboardHandle{ lName }); });
    }

    void FakeOnlineBackend::UploadScore(const OnlineLeaderboardHandle& InBoard, const Int32 InScore,
                                        const TDynArray<Int32>& InDetails, FOnScoreUploaded InDone)
    {
        FakeBoard* lBoard = FindBoard(InBoard.Value);
        if (lBoard == nullptr)
        {
            Reply([InDone = Move(InDone)]() { InDone(EOnlineResult::NotFound, {}); });
            return;
        }

        const ScoreUploadResult lResult = KeepBest(*lBoard, m_Players.front().Id, InScore, InDetails);
        Reply([InDone = Move(InDone), lResult]() { InDone(EOnlineResult::Ok, lResult); });
    }

    void FakeOnlineBackend::DownloadEntries(const OnlineLeaderboardHandle& InBoard, const ELeaderboardRange InRange,
                                            const Int32 InFirst, const Int32 InLast, FOnEntries InDone)
    {
        const FakeBoard* lBoard = FindBoard(InBoard.Value);
        if (lBoard == nullptr)
        {
            Reply([InDone = Move(InDone)]() { InDone(EOnlineResult::NotFound, {}); });
            return;
        }

        const TDynArray<LeaderboardEntry>& lAll   = lBoard->Entries;
        const OnlinePlayerId&              lLocal = m_Players.front().Id;
        const auto lMine = Algo::FindIf(lAll, [&lLocal](const LeaderboardEntry& InEntry) { return InEntry.Player.Id == lLocal; });
        const Int64 lMineIndex = (lMine != lAll.end()) ? static_cast<Int64>(lMine - lAll.begin()) : -1;

        const OnlineLeaderboardRules::IndexRange lRange =
            OnlineLeaderboardRules::SelectRange(lAll.size(), InRange, InFirst, InLast, lMineIndex);

        TDynArray<LeaderboardEntry> lEntries;
        for (Uint64 lIndex = lRange.Begin; lIndex < lRange.End; ++lIndex)
        {
            const LeaderboardEntry& lEntry = lAll[lIndex];
            const FakePlayer*       lOwner = FindPlayer(lEntry.Player.Id);
            if (InRange == ELeaderboardRange::Friends && lEntry.Player.Id != lLocal && (lOwner == nullptr || !lOwner->bFriend))
            {
                continue;
            }
            lEntries.push_back(lEntry);
        }

        Reply([InDone = Move(InDone), lEntries = WithNames(Move(lEntries))]() { InDone(EOnlineResult::Ok, lEntries); });
    }

    void FakeOnlineBackend::DownloadEntriesForPlayers(const OnlineLeaderboardHandle& InBoard,
                                                      const TDynArray<OnlinePlayerId>& InPlayers, FOnEntries InDone)
    {
        const FakeBoard* lBoard = FindBoard(InBoard.Value);
        if (lBoard == nullptr)
        {
            Reply([InDone = Move(InDone)]() { InDone(EOnlineResult::NotFound, {}); });
            return;
        }

        TDynArray<LeaderboardEntry> lEntries;
        for (const LeaderboardEntry& lEntry : lBoard->Entries)
        {
            if (Algo::Contains(InPlayers, lEntry.Player.Id))
            {
                lEntries.push_back(lEntry);
            }
        }

        Reply([InDone = Move(InDone), lEntries = WithNames(Move(lEntries))]() { InDone(EOnlineResult::Ok, lEntries); });
    }

    // =========================================================================
    // IOnlineFiles
    // =========================================================================
    void FakeOnlineBackend::AttachFile(const OnlineLeaderboardHandle& InBoard, const TDynArray<Uint8>& InBytes,
                                       FOnOnlineDone InDone)
    {
        FakeBoard* lBoard = FindBoard(InBoard.Value);
        const OnlinePlayerId& lLocal = m_Players.front().Id;
        LeaderboardEntry* lEntry = nullptr;
        if (lBoard != nullptr)
        {
            const auto lFound = Algo::FindIf(lBoard->Entries, [&lLocal](const LeaderboardEntry& InEntry) { return InEntry.Player.Id == lLocal; });
            lEntry = (lFound != lBoard->Entries.end()) ? &*lFound : nullptr;
        }

        EOnlineResult lResult = EOnlineResult::Ok;
        if (lEntry == nullptr)
        {
            lResult = EOnlineResult::NotFound;
        }
        else if (InBytes.size() > m_Capabilities.MaxFileBytes)
        {
            lResult = EOnlineResult::QuotaExceeded;
        }
        else
        {
            // A new id for the new file, as a store shares each one anew; the old one is gone.
            if (lEntry->File.IsValid())
            {
                m_Files.erase(lEntry->File.Value);
            }
            lEntry->File = OnlineFileId{ OpaaxString::Format("fakefile:{}", m_NextFile++) };
            m_Files[lEntry->File.Value] = InBytes;
        }

        Reply([InDone = Move(InDone), lResult]() { InDone(lResult); });
    }

    void FakeOnlineBackend::DownloadFile(const OnlineFileId& InFile, FOnFile InDone)
    {
        const auto lFound = m_Files.find(InFile.Value);
        if (lFound == m_Files.end())
        {
            Reply([InDone = Move(InDone)]() { InDone(EOnlineResult::NotFound, {}); });
            return;
        }

        Reply([InDone = Move(InDone), lBytes = lFound->second]() { InDone(EOnlineResult::Ok, lBytes); });
    }

    // =========================================================================
    // IOnlineAchievements
    // =========================================================================
    void FakeOnlineBackend::Unlock(const OpaaxString& InName, FOnOnlineDone InDone)
    {
        const auto lFound = Algo::FindIf(m_Achievements, [&InName](const OnlineAchievement& InAchievement)
        {
            return InAchievement.Name == InName;
        });
        if (lFound != m_Achievements.end())
        {
            lFound->bUnlocked = true;
        }
        else
        {
            m_Achievements.push_back(OnlineAchievement{ InName, true });
        }

        Reply([InDone = Move(InDone)]() { InDone(EOnlineResult::Ok); });
    }

    void FakeOnlineBackend::IndicateProgress(const OpaaxString&, Uint32, Uint32, FOnOnlineDone InDone)
    {
        Reply([InDone = Move(InDone)]() { InDone(EOnlineResult::Ok); });
    }

    void FakeOnlineBackend::GetAchievements(FOnAchievements InDone)
    {
        Reply([InDone = Move(InDone), lAchievements = m_Achievements]() { InDone(EOnlineResult::Ok, lAchievements); });
    }

    void FakeOnlineBackend::Clear(const OpaaxString& InName, FOnOnlineDone InDone)
    {
        const auto lFound = Algo::FindIf(m_Achievements, [&InName](const OnlineAchievement& InAchievement)
        {
            return InAchievement.Name == InName;
        });
        const bool bFound = lFound != m_Achievements.end();
        if (bFound)
        {
            lFound->bUnlocked = false;
        }

        Reply([InDone = Move(InDone), bFound]() { InDone(bFound ? EOnlineResult::Ok : EOnlineResult::NotFound); });
    }

    // =========================================================================
    // IOnlineOverlay
    // =========================================================================
    bool FakeOnlineBackend::Open(EOverlayPage, const OnlinePlayerId&)
    {
        SetOverlayActive(true);
        return true;
    }
}
