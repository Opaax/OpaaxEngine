#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Online/OnlineTypes.h"

namespace Opaax
{
    // =============================================================================
    // Callbacks a backend answers through. A backend may call them inside the call or at a later Pump;
    //   OnlineServices delivers them to the game at its own next Pump either way.
    // =============================================================================
    using FOnOnlineDone        = TFunction<void(EOnlineResult)>;
    using FOnLeaderboardFound  = TFunction<void(EOnlineResult, const OnlineLeaderboardHandle&)>;
    using FOnScoreUploaded     = TFunction<void(EOnlineResult, const ScoreUploadResult&)>;
    using FOnEntries           = TFunction<void(EOnlineResult, const TDynArray<LeaderboardEntry>&)>;
    using FOnFile              = TFunction<void(EOnlineResult, const TDynArray<Uint8>&)>;
    using FOnAchievements      = TFunction<void(EOnlineResult, const TDynArray<OnlineAchievement>&)>;

    // =============================================================================
    // Feature interfaces — what a store can do, one interface per feature. A backend gives the ones
    //   its store has; for the others the services answer Unsupported. Called on the main thread only.
    // =============================================================================

    /** The signed-in player, other players' names, the friends list. */
    class IOnlineIdentity
    {
    public:
        virtual ~IOnlineIdentity() = default;

        virtual OnlinePlayer GetLocalPlayer() const = 0;

        /** InId's name, empty while the store has not said. */
        virtual OpaaxString GetPlayerName(const OnlinePlayerId& InId) const = 0;

        /** Asks the store for names it does not know yet; Ok once they are known. */
        virtual void RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone) = 0;

        virtual TDynArray<OnlinePlayerId> GetFriends() const = 0;
    };

    /** Leaderboards: found by their definition, scores kept at the player's best. */
    class IOnlineLeaderboards
    {
    public:
        virtual ~IOnlineLeaderboards() = default;

        /** The store's leaderboard for InDefinition, made if the store makes them at run time. */
        virtual void FindLeaderboard(const LeaderboardDefinition& InDefinition, FOnLeaderboardFound InDone) = 0;

        /** Uploads InScore, keeping the player's best by the board's sort. */
        virtual void UploadScore(const OnlineLeaderboardHandle& InBoard, Int32 InScore, const TDynArray<Int32>& InDetails,
                                 FOnScoreUploaded InDone) = 0;

        /** The entries of InRange (see ELeaderboardRange for First and Last). */
        virtual void DownloadEntries(const OnlineLeaderboardHandle& InBoard, ELeaderboardRange InRange, Int32 InFirst,
                                     Int32 InLast, FOnEntries InDone) = 0;

        /** The entries of these players (those who have one), with their ranks on the whole board. */
        virtual void DownloadEntriesForPlayers(const OnlineLeaderboardHandle& InBoard, const TDynArray<OnlinePlayerId>& InPlayers,
                                               FOnEntries InDone) = 0;
    };

    /** Files attached to leaderboard entries (ghosts). */
    class IOnlineFiles
    {
    public:
        virtual ~IOnlineFiles() = default;

        /** Attaches InBytes to the player's entry on InBoard, replacing its file. */
        virtual void AttachFile(const OnlineLeaderboardHandle& InBoard, const TDynArray<Uint8>& InBytes, FOnOnlineDone InDone) = 0;

        virtual void DownloadFile(const OnlineFileId& InFile, FOnFile InDone) = 0;
    };

    /** The player's achievements. */
    class IOnlineAchievements
    {
    public:
        virtual ~IOnlineAchievements() = default;

        virtual void Unlock(const OpaaxString& InName, FOnOnlineDone InDone) = 0;

        /** Shows the store's progress notice (InCurrent of InMax); unlocking stays the game's call. */
        virtual void IndicateProgress(const OpaaxString& InName, Uint32 InCurrent, Uint32 InMax, FOnOnlineDone InDone) = 0;

        virtual void GetAchievements(FOnAchievements InDone) = 0;

        /** Locks InName again. For development and tests: a shipped game never calls it. */
        virtual void Clear(const OpaaxString& InName, FOnOnlineDone InDone) = 0;
    };

    /** The store's overlay. Its opening and closing come through IOnlineBackend::NotifyOverlayChanged. */
    class IOnlineOverlay
    {
    public:
        virtual ~IOnlineOverlay() = default;

        virtual bool IsActive() const = 0;

        /** Opens InPage (InPlayer for a profile). False when the store cannot show it now. */
        virtual bool Open(EOverlayPage InPage, const OnlinePlayerId& InPlayer) = 0;
    };

    // =============================================================================
    // IOnlineBackend — one store. Made by OnlineAPI, owned by OnlineServices, used on the main thread.
    // =============================================================================
    class IOnlineBackend
    {
    public:
        virtual ~IOnlineBackend() = default;

        virtual EOnlineBackend     GetType() const noexcept = 0;
        virtual OnlineCapabilities GetCapabilities() const = 0;

        /** Runs the store's callbacks. Once a frame, from OnlineServices::Pump. */
        virtual void Pump() {}

        /** Stops the store. Nothing is called on the backend after. */
        virtual void Shutdown() {}

        // The features this store has; null for the others.
        virtual IOnlineIdentity*     GetIdentity()     { return nullptr; }
        virtual IOnlineLeaderboards* GetLeaderboards() { return nullptr; }
        virtual IOnlineFiles*        GetFiles()        { return nullptr; }
        virtual IOnlineAchievements* GetAchievements() { return nullptr; }
        virtual IOnlineOverlay*      GetOverlay()      { return nullptr; }

        /** Who hears the overlay open and close (OnlineServices). */
        void SetOverlayListener(TFunction<void(bool)> InListener) { m_OverlayListener = Move(InListener); }

    protected:
        /** Called by a backend when its overlay opens (true) or closes. */
        void NotifyOverlayChanged(const bool bInActive) const
        {
            if (m_OverlayListener)
            {
                m_OverlayListener(bInActive);
            }
        }

    private:
        TFunction<void(bool)> m_OverlayListener;
    };
}
