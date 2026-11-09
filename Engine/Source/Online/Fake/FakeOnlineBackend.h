#pragma once

#include "Core/Hash/OpaaxHash.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Online/IOnlineBackend.h"

namespace Opaax
{
    // =============================================================================
    // EFakeStoreProfile — which store the fake one behaves as: the features it has and lacks. A game is
    //   tried against what a store lacks without that store.
    // =============================================================================
    enum class EFakeStoreProfile : Uint8
    {
        /** Every feature. */
        Full,

        /** Leaderboards made at run time, a friends range, files on entries. */
        Steam,

        /** Leaderboards only as defined in the store's portal, no friends range, no files. */
        Epic,

        /** Leaderboards made at run time, files on entries, no friends range. */
        Gog
    };

    constexpr const char* ToString(const EFakeStoreProfile InProfile) noexcept
    {
        switch (InProfile)
        {
            case EFakeStoreProfile::Full:  return "full";
            case EFakeStoreProfile::Steam: return "steam";
            case EFakeStoreProfile::Epic:  return "epic";
            case EFakeStoreProfile::Gog:   return "gog";
        }
        return "unknown";
    }

    /** InText's profile ("full", "steam", "epic", "gog", any case). False when it names none. */
    bool ParseFakeStoreProfile(OpaaxStringView InText, EFakeStoreProfile& OutProfile);

    /** What the fake store can do as InProfile. The sizes are the fake's: a real backend gives its store's. */
    OnlineCapabilities GetFakeStoreCapabilities(EFakeStoreProfile InProfile);

    // =============================================================================
    // FakeOnlineBackend — a store kept in memory: players and friends, leaderboards with the stores'
    //   rules (OnlineLeaderboardRules), files on entries, achievements, an overlay opened from code.
    //   For unit tests, feature tests (--online fake) and making a game's online screens without a
    //   store. It answers inside each call, or a number of pumps later (SetLatency).
    // =============================================================================
    class FakeOnlineBackend final : public IOnlineBackend, public IOnlineIdentity, public IOnlineLeaderboards,
                                    public IOnlineFiles, public IOnlineAchievements, public IOnlineOverlay
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        /** A store with one player, "Player", signed in. */
        explicit FakeOnlineBackend(EFakeStoreProfile InProfile = EFakeStoreProfile::Full);

        // =============================================================================
        // Setup — what a test or automation puts in the store
        // =============================================================================
    public:
        EFakeStoreProfile GetProfile() const noexcept { return m_Profile; }

        /** Answers come InPumps pumps after the call; 0 answers inside it. */
        void SetLatency(Uint32 InPumps) noexcept { m_Latency = InPumps; }

        /**
         * Adds a player.
         * @param bInFriend     A friend of the signed-in player
         * @param bInNameKnown  False: the store does not know the name until RequestPlayerNames
         */
        OnlinePlayerId AddPlayer(const OpaaxString& InName, bool bInFriend, bool bInNameKnown = true);

        /** False: RequestPlayerNames never answers, as a store that does not reply. */
        void SetNamesAnswer(bool bInAnswers) noexcept { m_bNamesAnswer = bInAnswers; }

        /** A leaderboard made in the store's portal: the only kind a store that makes none at run time finds. */
        void DefineLeaderboard(const LeaderboardDefinition& InDefinition);

        /**
         * InPlayer's score on InBoard, kept at their best, as if they had uploaded it.
         * @return False when the board does not exist
         */
        bool SetScore(const OpaaxString& InBoard, const OnlinePlayerId& InPlayer, Int32 InScore,
                      const TDynArray<Int32>& InDetails = {});

        /** Opens or closes the overlay, as the player's overlay key does. */
        void SetOverlayActive(bool bInActive);

        // =============================================================================
        // Read — what the store holds
        // =============================================================================
    public:
        /** InBoard's entries, best first; null when the board does not exist. Names as the store knows them. */
        const TDynArray<LeaderboardEntry>* FindEntries(const OpaaxString& InBoard) const;

        bool IsAchievementUnlocked(const OpaaxString& InName) const;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IOnlineBackend interface
    public:
        EOnlineBackend     GetType() const noexcept override { return EOnlineBackend::Fake; }
        OnlineCapabilities GetCapabilities() const override  { return m_Capabilities; }
        void               Pump() override;

        IOnlineIdentity*     GetIdentity() override     { return this; }
        IOnlineLeaderboards* GetLeaderboards() override { return this; }
        IOnlineFiles*        GetFiles() override        { return m_Capabilities.bLeaderboardFiles ? this : nullptr; }
        IOnlineAchievements* GetAchievements() override { return this; }
        IOnlineOverlay*      GetOverlay() override      { return this; }
        //~End IOnlineBackend interface

        //~Begin IOnlineIdentity interface
    public:
        OnlinePlayer              GetLocalPlayer() const override;
        OpaaxString               GetPlayerName(const OnlinePlayerId& InId) const override;
        void                      RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone) override;
        TDynArray<OnlinePlayerId> GetFriends() const override;
        //~End IOnlineIdentity interface

        //~Begin IOnlineLeaderboards interface
    public:
        void FindLeaderboard(const LeaderboardDefinition& InDefinition, FOnLeaderboardFound InDone) override;
        void UploadScore(const OnlineLeaderboardHandle& InBoard, Int32 InScore, const TDynArray<Int32>& InDetails,
                         FOnScoreUploaded InDone) override;
        void DownloadEntries(const OnlineLeaderboardHandle& InBoard, ELeaderboardRange InRange, Int32 InFirst, Int32 InLast,
                             FOnEntries InDone) override;
        void DownloadEntriesForPlayers(const OnlineLeaderboardHandle& InBoard, const TDynArray<OnlinePlayerId>& InPlayers,
                                       FOnEntries InDone) override;
        //~End IOnlineLeaderboards interface

        //~Begin IOnlineFiles interface
    public:
        void AttachFile(const OnlineLeaderboardHandle& InBoard, const TDynArray<Uint8>& InBytes, FOnOnlineDone InDone) override;
        void DownloadFile(const OnlineFileId& InFile, FOnFile InDone) override;
        //~End IOnlineFiles interface

        //~Begin IOnlineAchievements interface
    public:
        void Unlock(const OpaaxString& InName, FOnOnlineDone InDone) override;
        void IndicateProgress(const OpaaxString& InName, Uint32 InCurrent, Uint32 InMax, FOnOnlineDone InDone) override;
        void GetAchievements(FOnAchievements InDone) override;
        void Clear(const OpaaxString& InName, FOnOnlineDone InDone) override;
        //~End IOnlineAchievements interface

        //~Begin IOnlineOverlay interface
    public:
        bool IsActive() const override { return m_bOverlayActive; }
        bool Open(EOverlayPage InPage, const OnlinePlayerId& InPlayer) override;
        //~End IOnlineOverlay interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        struct FakePlayer
        {
            OnlinePlayerId Id;
            OpaaxString    Name;
            bool           bFriend    = false;
            bool           bNameKnown = true;
        };

        struct FakeBoard
        {
            LeaderboardDefinition       Definition;

            /** Best first, ranked; names left empty (filled when read). */
            TDynArray<LeaderboardEntry> Entries;
        };

        /** Runs InAnswer now, or after the latency. */
        void Reply(TFunction<void()> InAnswer);

        const FakePlayer* FindPlayer(const OnlinePlayerId& InId) const;
        FakeBoard*        FindBoard(const OpaaxString& InName);

        /** InEntries with each player's name as the store knows it. */
        TDynArray<LeaderboardEntry> WithNames(TDynArray<LeaderboardEntry> InEntries) const;

        /** Keeps InScore as InPlayer's entry if it beats their best. */
        ScoreUploadResult KeepBest(FakeBoard& InOutBoard, const OnlinePlayerId& InPlayer, Int32 InScore,
                                   const TDynArray<Int32>& InDetails);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EFakeStoreProfile  m_Profile;
        OnlineCapabilities m_Capabilities;

        Uint32 m_Latency      = 0;
        bool   m_bNamesAnswer = true;

        /** The signed-in player is the first. */
        TDynArray<FakePlayer> m_Players;

        TUnorderedMap<OpaaxString, FakeBoard>        m_Boards;
        TUnorderedMap<OpaaxString, TDynArray<Uint8>> m_Files;
        Uint64                                       m_NextFile = 1;

        TDynArray<OnlineAchievement> m_Achievements;
        bool                         m_bOverlayActive = false;

        /** Answers waiting for their pump: pumps left, then the call. */
        TDynArray<TPair<Uint32, TFunction<void()>>> m_Delayed;
    };
}
