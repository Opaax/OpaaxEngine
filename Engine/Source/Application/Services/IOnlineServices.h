#pragma once

#include "Application/Services/IAppService.h"
#include "Core/Events/Delegate.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Online/IOnlineBackend.h"

namespace Opaax
{
    inline constexpr LogCategory LogOnline{"Online"};

    // =============================================================================
    // IOnlineServices — the player's store (Steam...) as one set of calls: leaderboards with a file on
    //   each entry, achievements, names, the overlay. The same calls whatever the store, and whether
    //   there is one: with none, every request answers Unavailable and the game plays on. A store that
    //   lacks a feature answers Unsupported; GetCapabilities says so up front.
    //   Requests answer at a later Pump (once a frame, in the engine loop), never inside the call, on
    //   the main thread. Called on the main thread only.
    // =============================================================================
    class IOnlineServices : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IOnlineServices)

        // =============================================================================
        // Store
        // =============================================================================
    public:
        virtual EOnlineBackend     GetBackend() const noexcept = 0;

        /** Why this backend runs ("Steam is not running"...), for logs and online.info. */
        virtual const OpaaxString& GetBackendReason() const noexcept = 0;

        virtual OnlineCapabilities GetCapabilities() const = 0;

        /** True when a store runs: requests can do something. */
        bool IsAvailable() const noexcept { return GetBackend() != EOnlineBackend::None; }

        // =============================================================================
        // Players
        // =============================================================================
    public:
        /** The signed-in player; an invalid id when no store runs. */
        virtual OnlinePlayer GetLocalPlayer() const = 0;

        /** InId's name, empty while the store has not said. */
        virtual OpaaxString GetPlayerName(const OnlinePlayerId& InId) const = 0;

        /** Asks the store for these players' names; read them with GetPlayerName once Ok. */
        virtual OnlineRequestId RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone) = 0;

        // =============================================================================
        // Leaderboards — named by the game's definitions (SetLeaderboards)
        // =============================================================================
    public:
        /** The game's leaderboards. A board not listed answers NotFound. */
        virtual void SetLeaderboards(const TDynArray<LeaderboardDefinition>& InDefinitions) = 0;
        virtual const TDynArray<LeaderboardDefinition>& GetLeaderboards() const = 0;

        /**
         * Uploads InScore to InBoard, keeping the player's best. With InFile, the file is attached to the
         * entry only when the score was kept, so the best entry keeps its own file.
         * @param InDone Ok with what the upload did; Unsupported when a file is given and the store keeps none
         */
        virtual OnlineRequestId UploadScore(OpaaxStringView InBoard, Int32 InScore, const TDynArray<Int32>& InDetails,
                                            const TDynArray<Uint8>& InFile, FOnScoreUploaded InDone) = 0;

        /**
         * InBoard's entries in InRange (see ELeaderboardRange for First and Last), with the players' names.
         * Names the store does not know yet are asked for; the answer waits for them a few seconds at
         * most, then comes with an empty name for those still unknown.
         */
        virtual OnlineRequestId DownloadEntries(OpaaxStringView InBoard, ELeaderboardRange InRange, Int32 InFirst,
                                                Int32 InLast, FOnEntries InDone) = 0;

        /** A file attached to an entry (LeaderboardEntry::File). */
        virtual OnlineRequestId DownloadFile(const OnlineFileId& InFile, FOnFile InDone) = 0;

        // =============================================================================
        // Achievements
        // =============================================================================
    public:
        virtual OnlineRequestId UnlockAchievement(const OpaaxString& InName, FOnOnlineDone InDone) = 0;

        /** Shows the store's progress notice (InCurrent of InMax); unlocking stays the game's call. */
        virtual OnlineRequestId IndicateAchievementProgress(const OpaaxString& InName, Uint32 InCurrent, Uint32 InMax,
                                                            FOnOnlineDone InDone) = 0;

        virtual OnlineRequestId GetAchievements(FOnAchievements InDone) = 0;

        /** Locks InName again. For development and tests: a shipped game never calls it. */
        virtual OnlineRequestId ClearAchievement(const OpaaxString& InName, FOnOnlineDone InDone) = 0;

        // =============================================================================
        // Overlay
        // =============================================================================
    public:
        virtual bool IsOverlayActive() const = 0;

        /** Opens the store's overlay on InPage. False when there is none, or it cannot show it now. */
        virtual bool OpenOverlay(EOverlayPage InPage, const OnlinePlayerId& InPlayer = {}) = 0;

        /** Broadcast when the overlay opens (true) and closes: a game pauses there. */
        virtual TMulticastDelegate<bool>& OnOverlayChanged() = 0;

        // =============================================================================
        // Requests
        // =============================================================================
    public:
        /** InRequest's callback never runs. Nothing when it already ran. */
        virtual void Cancel(OnlineRequestId InRequest) = 0;

        /** Requests not answered yet. */
        virtual Uint32 GetPendingCount() const = 0;

        /** Runs the store's callbacks, then answers the requests that are done. Once a frame. */
        virtual void Pump() = 0;

        //----- null object ----------------------------------------------------
        /** No store and nothing pumped: a request answers nothing and returns INVALID_ONLINE_REQUEST. */
        static IOnlineServices& Null();
    };

    // =============================================================================
    // OnlineServices — the online services over one backend (a store, or none).
    // =============================================================================
    class OnlineServices final : public IOnlineServices
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        /** Over InBackend (never null), which InReason explains ("no store built in"...). */
        OnlineServices(TUniquePtr<IOnlineBackend> InBackend, OpaaxString InReason);
        ~OnlineServices() override;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IAppService interface
    public:
        void OnShutdown() override;
        //~End IAppService interface

        //~Begin IOnlineServices interface
    public:
        EOnlineBackend     GetBackend() const noexcept override { return m_Backend->GetType(); }
        const OpaaxString& GetBackendReason() const noexcept override { return m_Reason; }
        OnlineCapabilities GetCapabilities() const override { return m_Backend->GetCapabilities(); }

        OnlinePlayer    GetLocalPlayer() const override;
        OpaaxString     GetPlayerName(const OnlinePlayerId& InId) const override;
        OnlineRequestId RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone) override;

        void SetLeaderboards(const TDynArray<LeaderboardDefinition>& InDefinitions) override;
        const TDynArray<LeaderboardDefinition>& GetLeaderboards() const override { return m_Definitions; }

        OnlineRequestId UploadScore(OpaaxStringView InBoard, Int32 InScore, const TDynArray<Int32>& InDetails,
                                    const TDynArray<Uint8>& InFile, FOnScoreUploaded InDone) override;
        OnlineRequestId DownloadEntries(OpaaxStringView InBoard, ELeaderboardRange InRange, Int32 InFirst, Int32 InLast,
                                        FOnEntries InDone) override;
        OnlineRequestId DownloadFile(const OnlineFileId& InFile, FOnFile InDone) override;

        OnlineRequestId UnlockAchievement(const OpaaxString& InName, FOnOnlineDone InDone) override;
        OnlineRequestId IndicateAchievementProgress(const OpaaxString& InName, Uint32 InCurrent, Uint32 InMax,
                                                    FOnOnlineDone InDone) override;
        OnlineRequestId GetAchievements(FOnAchievements InDone) override;
        OnlineRequestId ClearAchievement(const OpaaxString& InName, FOnOnlineDone InDone) override;

        bool                      IsOverlayActive() const override { return m_bOverlayActive; }
        bool                      OpenOverlay(EOverlayPage InPage, const OnlinePlayerId& InPlayer = {}) override;
        TMulticastDelegate<bool>& OnOverlayChanged() override { return m_OnOverlayChanged; }

        void   Cancel(OnlineRequestId InRequest) override;
        Uint32 GetPendingCount() const override { return static_cast<Uint32>(m_Live.size()); }
        void   Pump() override;
        //~End IOnlineServices interface

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        /** The backend, for tests and automation that drive a fake store. */
        IOnlineBackend& GetBackendObject() noexcept { return *m_Backend; }

        /** How long a download waits for names the store does not know yet (3 s); then it answers with those it has. */
        void SetNameTimeout(const double InSeconds) noexcept { m_NameTimeout = InSeconds; }

        /** The clock the name timeout reads (Clock::NowSeconds); a test gives its own. */
        void SetClock(TFunction<double()> InClock) { m_Clock = Move(InClock); }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** A new request id, live until answered or cancelled. */
        OnlineRequestId Begin();

        /** Queues InAnswer for InRequest's answer at the next Pump. */
        void Answer(OnlineRequestId InRequest, TFunction<void()> InAnswer);

        /** True while InRequest is neither answered nor cancelled. */
        bool IsLive(const OnlineRequestId InRequest) const { return m_Live.count(InRequest) > 0; }

        /** A backend callback that queues InDone's call, with what the backend gave, as InRequest's answer. */
        template<typename TDone>
        auto Relay(OnlineRequestId InRequest, TDone InDone);

        /** Queues InDone's call with InResult and empty values as InRequest's answer. */
        template<typename... TArgs>
        void Fail(OnlineRequestId InRequest, const TFunction<void(EOnlineResult, TArgs...)>& InDone, EOnlineResult InResult);

        /** Unavailable or Unsupported for a feature the backend lacks; Ok when it has it. */
        EOnlineResult CheckFeature(const void* InFeature) const;

        /** Finds InBoard's handle (cached), then calls InThen with it. */
        void WithBoard(OnlineRequestId InRequest, OpaaxStringView InBoard,
                       TFunction<void(EOnlineResult, const OnlineLeaderboardHandle&)> InThen);

        /** Answers downloaded entries once their names are known, or once the name timeout passes. */
        void CompleteEntries(OnlineRequestId InRequest, EOnlineResult InResult, TDynArray<LeaderboardEntry> InEntries,
                             FOnEntries InDone);

        /** Answers InRequest's name wait with the names known now. */
        void FinishNameWait(OnlineRequestId InRequest);

        /** Fills the entries' empty names with those the store knows. */
        void FillNames(TDynArray<LeaderboardEntry>& InOutEntries) const;

        double Now() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IOnlineBackend> m_Backend;
        OpaaxString                m_Reason;

        TDynArray<LeaderboardDefinition>                      m_Definitions;
        TUnorderedMap<OpaaxString, OnlineLeaderboardHandle>  m_Boards;

        OnlineRequestId                  m_NextRequest = 1;
        TUnorderedSet<OnlineRequestId>   m_Live;
        TDynArray<TPair<OnlineRequestId, TFunction<void()>>> m_Ready;

        bool                     m_bOverlayActive = false;
        TMulticastDelegate<bool> m_OnOverlayChanged;

        /** Downloads waiting for names. */
        struct NameWait
        {
            OnlineRequestId             Request  = INVALID_ONLINE_REQUEST;
            double                      Deadline = 0.0;
            TDynArray<LeaderboardEntry> Entries;
            FOnEntries                  Done;
        };
        TDynArray<NameWait> m_NameWaits;
        double              m_NameTimeout = 3.0;
        TFunction<double()> m_Clock;
    };
}
