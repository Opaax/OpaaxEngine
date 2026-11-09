#include "Application/Services/IOnlineServices.h"

#include "Core/Templates/TypeTraits.h"
#include "Online/Null/NullOnlineBackend.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullOnlineServices — nothing at all: no store, and never pumped, so it answers nothing.
        // =====================================================================
        class NullOnlineServices final : public IOnlineServices
        {
        public:
            bool               IsNull() const noexcept override                    { return true; }
            EOnlineBackend     GetBackend() const noexcept override                { return EOnlineBackend::None; }
            const OpaaxString& GetBackendReason() const noexcept override          { return m_Reason; }
            OnlineCapabilities GetCapabilities() const override                    { return {}; }
            OnlinePlayer       GetLocalPlayer() const override                     { return {}; }
            OpaaxString        GetPlayerName(const OnlinePlayerId&) const override { return {}; }

            OnlineRequestId RequestPlayerNames(const TDynArray<OnlinePlayerId>&, FOnOnlineDone) override { return INVALID_ONLINE_REQUEST; }

            void SetLeaderboards(const TDynArray<LeaderboardDefinition>&) override {}
            const TDynArray<LeaderboardDefinition>& GetLeaderboards() const override { return m_Definitions; }

            OnlineRequestId UploadScore(OpaaxStringView, Int32, const TDynArray<Int32>&, const TDynArray<Uint8>&,
                                        FOnScoreUploaded) override { return INVALID_ONLINE_REQUEST; }
            OnlineRequestId DownloadEntries(OpaaxStringView, ELeaderboardRange, Int32, Int32, FOnEntries) override
            {
                return INVALID_ONLINE_REQUEST;
            }
            OnlineRequestId DownloadFile(const OnlineFileId&, FOnFile) override { return INVALID_ONLINE_REQUEST; }

            OnlineRequestId UnlockAchievement(const OpaaxString&, FOnOnlineDone) override { return INVALID_ONLINE_REQUEST; }
            OnlineRequestId IndicateAchievementProgress(const OpaaxString&, Uint32, Uint32, FOnOnlineDone) override
            {
                return INVALID_ONLINE_REQUEST;
            }
            OnlineRequestId GetAchievements(FOnAchievements) override { return INVALID_ONLINE_REQUEST; }
            OnlineRequestId ClearAchievement(const OpaaxString&, FOnOnlineDone) override { return INVALID_ONLINE_REQUEST; }

            bool                      IsOverlayActive() const override                         { return false; }
            bool                      OpenOverlay(EOverlayPage, const OnlinePlayerId&) override { return false; }
            TMulticastDelegate<bool>& OnOverlayChanged() override                              { return m_OnOverlayChanged; }

            void   Cancel(OnlineRequestId) override {}
            Uint32 GetPendingCount() const override { return 0; }
            void   Pump() override {}

        private:
            OpaaxString                      m_Reason = "no online services";
            TDynArray<LeaderboardDefinition> m_Definitions;
            TMulticastDelegate<bool>         m_OnOverlayChanged;
        };
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
    // =========================================================================
    ServiceTypeID IOnlineServices::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IOnlineServices& IOnlineServices::Null()
    {
        static NullOnlineServices s_Null;
        return s_Null;
    }

    // =========================================================================
    // OnlineServices — helpers
    // =========================================================================
    template<typename TDone>
    auto OnlineServices::Relay(const OnlineRequestId InRequest, TDone InDone)
    {
        // The values are copied: the backend's may not outlive its callback.
        return [this, InRequest, InDone = Move(InDone)](const EOnlineResult InResult, const auto&... InValues)
        {
            Answer(InRequest, [InDone, InResult, InValues...]()
            {
                if (InDone)
                {
                    InDone(InResult, InValues...);
                }
            });
        };
    }

    template<typename... TArgs>
    void OnlineServices::Fail(const OnlineRequestId InRequest, const TFunction<void(EOnlineResult, TArgs...)>& InDone,
                              const EOnlineResult InResult)
    {
        Answer(InRequest, [InDone, InResult]()
        {
            if (InDone)
            {
                InDone(InResult, TDecay<TArgs>{}...);
            }
        });
    }

    // =========================================================================
    // OnlineServices
    // =========================================================================
    OnlineServices::OnlineServices(TUniquePtr<IOnlineBackend> InBackend, OpaaxString InReason)
        : m_Backend(Move(InBackend))
        , m_Reason(Move(InReason))
    {
        if (m_Backend == nullptr)
        {
            m_Backend = MakeUnique<NullOnlineBackend>();
        }

        m_Backend->SetOverlayListener([this](const bool bInActive)
        {
            if (m_bOverlayActive == bInActive)
            {
                return;
            }

            m_bOverlayActive = bInActive;
            OPAAX_LOG(LogOnline, Info, "Overlay {}", bInActive ? "opened" : "closed");
            m_OnOverlayChanged.Broadcast(bInActive);
        });

        OPAAX_LOG(LogOnline, Info, "Online: {} ({})", ToString(m_Backend->GetType()), m_Reason.CStr());
    }

    OnlineServices::~OnlineServices()
    {
        OnShutdown();
    }

    void OnlineServices::OnShutdown()
    {
        if (m_Backend->GetType() == EOnlineBackend::None && m_Live.empty())
        {
            return;
        }

        // The store stops first: none of its callbacks comes after. What was asked is dropped, and later
        // requests answer Unavailable.
        m_Backend->Shutdown();
        m_Backend = MakeUnique<NullOnlineBackend>();
        m_Reason  = "shut down";
        m_Boards.clear();
        m_Live.clear();
        m_Ready.clear();
        m_bOverlayActive = false;
    }

    OnlineRequestId OnlineServices::Begin()
    {
        const OnlineRequestId lRequest = m_NextRequest++;
        m_Live.insert(lRequest);
        return lRequest;
    }

    void OnlineServices::Answer(const OnlineRequestId InRequest, TFunction<void()> InAnswer)
    {
        m_Ready.emplace_back(InRequest, Move(InAnswer));
    }

    EOnlineResult OnlineServices::CheckFeature(const void* InFeature) const
    {
        if (!IsAvailable())
        {
            return EOnlineResult::Unavailable;
        }
        return (InFeature != nullptr) ? EOnlineResult::Ok : EOnlineResult::Unsupported;
    }

    void OnlineServices::Pump()
    {
        m_Backend->Pump();

        // Answers queued while these run (a callback asking again) wait for the next Pump.
        TDynArray<TPair<OnlineRequestId, TFunction<void()>>> lReady;
        lReady.swap(m_Ready);

        for (TPair<OnlineRequestId, TFunction<void()>>& lAnswer : lReady)
        {
            if (m_Live.erase(lAnswer.first) > 0)
            {
                lAnswer.second();
            }
        }
    }

    void OnlineServices::Cancel(const OnlineRequestId InRequest)
    {
        m_Live.erase(InRequest);
    }

    // =========================================================================
    // Players
    // =========================================================================
    OnlinePlayer OnlineServices::GetLocalPlayer() const
    {
        const IOnlineIdentity* lIdentity = m_Backend->GetIdentity();
        return (lIdentity != nullptr) ? lIdentity->GetLocalPlayer() : OnlinePlayer{};
    }

    OpaaxString OnlineServices::GetPlayerName(const OnlinePlayerId& InId) const
    {
        const IOnlineIdentity* lIdentity = m_Backend->GetIdentity();
        return (lIdentity != nullptr) ? lIdentity->GetPlayerName(InId) : OpaaxString();
    }

    OnlineRequestId OnlineServices::RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone)
    {
        const OnlineRequestId lRequest = Begin();
        IOnlineIdentity* lIdentity = m_Backend->GetIdentity();

        if (const EOnlineResult lCheck = CheckFeature(lIdentity); lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }

        lIdentity->RequestPlayerNames(InIds, Relay(lRequest, Move(InDone)));
        return lRequest;
    }

    // =========================================================================
    // Leaderboards
    // =========================================================================
    void OnlineServices::SetLeaderboards(const TDynArray<LeaderboardDefinition>& InDefinitions)
    {
        m_Definitions = InDefinitions;
        m_Boards.clear();
    }

    void OnlineServices::WithBoard(const OnlineRequestId InRequest, const OpaaxStringView InBoard,
                                   TFunction<void(EOnlineResult, const OnlineLeaderboardHandle&)> InThen)
    {
        const OpaaxString lName(InBoard);

        if (const auto lFound = m_Boards.find(lName); lFound != m_Boards.end())
        {
            InThen(EOnlineResult::Ok, lFound->second);
            return;
        }

        const LeaderboardDefinition* lDefinition = nullptr;
        for (const LeaderboardDefinition& lCandidate : m_Definitions)
        {
            if (lCandidate.Name == lName)
            {
                lDefinition = &lCandidate;
                break;
            }
        }

        if (lDefinition == nullptr)
        {
            OPAAX_LOG(LogOnline, Warn, "Leaderboard '{}' is not one of the game's: add it to the online config's "
                                       "leaderboards", lName.CStr());
            InThen(EOnlineResult::NotFound, {});
            return;
        }

        m_Backend->GetLeaderboards()->FindLeaderboard(*lDefinition,
            [this, InRequest, lName, InThen = Move(InThen)](const EOnlineResult InResult, const OnlineLeaderboardHandle& InHandle)
            {
                if (InResult == EOnlineResult::Ok)
                {
                    m_Boards[lName] = InHandle;
                }
                if (IsLive(InRequest))
                {
                    InThen(InResult, InHandle);
                }
            });
    }

    OnlineRequestId OnlineServices::UploadScore(const OpaaxStringView InBoard, const Int32 InScore,
                                                const TDynArray<Int32>& InDetails, const TDynArray<Uint8>& InFile,
                                                FOnScoreUploaded InDone)
    {
        const OnlineRequestId lRequest = Begin();

        EOnlineResult lCheck = CheckFeature(m_Backend->GetLeaderboards());
        if (lCheck == EOnlineResult::Ok && !InFile.empty())
        {
            lCheck = CheckFeature(m_Backend->GetFiles());
        }
        if (lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }

        WithBoard(lRequest, InBoard,
            [this, lRequest, InScore, InDetails, InFile, InDone](const EOnlineResult InFound, const OnlineLeaderboardHandle& InHandle)
            {
                if (InFound != EOnlineResult::Ok)
                {
                    Fail(lRequest, InDone, InFound);
                    return;
                }

                m_Backend->GetLeaderboards()->UploadScore(InHandle, InScore, InDetails,
                    [this, lRequest, InHandle, InFile, InDone](const EOnlineResult InUploaded, const ScoreUploadResult& InUpload)
                    {
                        // The file goes with a kept score only: a worse run leaves the best entry's file.
                        if (InUploaded != EOnlineResult::Ok || !InUpload.bScoreChanged || InFile.empty() || !IsLive(lRequest))
                        {
                            Relay(lRequest, InDone)(InUploaded, InUpload);
                            return;
                        }

                        m_Backend->GetFiles()->AttachFile(InHandle, InFile,
                            [this, lRequest, InUpload, InDone](const EOnlineResult InAttached)
                            {
                                Relay(lRequest, InDone)(InAttached, InUpload);
                            });
                    });
            });
        return lRequest;
    }

    OnlineRequestId OnlineServices::DownloadEntries(const OpaaxStringView InBoard, const ELeaderboardRange InRange,
                                                    const Int32 InFirst, const Int32 InLast, FOnEntries InDone)
    {
        const OnlineRequestId lRequest = Begin();

        if (const EOnlineResult lCheck = CheckFeature(m_Backend->GetLeaderboards()); lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }

        WithBoard(lRequest, InBoard,
            [this, lRequest, InRange, InFirst, InLast, InDone](const EOnlineResult InFound, const OnlineLeaderboardHandle& InHandle)
            {
                if (InFound != EOnlineResult::Ok)
                {
                    Fail(lRequest, InDone, InFound);
                    return;
                }
                m_Backend->GetLeaderboards()->DownloadEntries(InHandle, InRange, InFirst, InLast, Relay(lRequest, InDone));
            });
        return lRequest;
    }

    OnlineRequestId OnlineServices::DownloadFile(const OnlineFileId& InFile, FOnFile InDone)
    {
        const OnlineRequestId lRequest = Begin();
        IOnlineFiles* lFiles = m_Backend->GetFiles();

        if (const EOnlineResult lCheck = CheckFeature(lFiles); lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }
        if (!InFile.IsValid())
        {
            Fail(lRequest, InDone, EOnlineResult::NotFound);
            return lRequest;
        }

        lFiles->DownloadFile(InFile, Relay(lRequest, Move(InDone)));
        return lRequest;
    }

    // =========================================================================
    // Achievements
    // =========================================================================
    OnlineRequestId OnlineServices::UnlockAchievement(const OpaaxString& InName, FOnOnlineDone InDone)
    {
        const OnlineRequestId lRequest = Begin();
        IOnlineAchievements* lAchievements = m_Backend->GetAchievements();

        if (const EOnlineResult lCheck = CheckFeature(lAchievements); lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }

        lAchievements->Unlock(InName, Relay(lRequest, Move(InDone)));
        return lRequest;
    }

    OnlineRequestId OnlineServices::IndicateAchievementProgress(const OpaaxString& InName, const Uint32 InCurrent,
                                                                const Uint32 InMax, FOnOnlineDone InDone)
    {
        const OnlineRequestId lRequest = Begin();
        IOnlineAchievements* lAchievements = m_Backend->GetAchievements();

        if (const EOnlineResult lCheck = CheckFeature(lAchievements); lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }

        lAchievements->IndicateProgress(InName, InCurrent, InMax, Relay(lRequest, Move(InDone)));
        return lRequest;
    }

    OnlineRequestId OnlineServices::GetAchievements(FOnAchievements InDone)
    {
        const OnlineRequestId lRequest = Begin();
        IOnlineAchievements* lAchievements = m_Backend->GetAchievements();

        if (const EOnlineResult lCheck = CheckFeature(lAchievements); lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }

        lAchievements->GetAchievements(Relay(lRequest, Move(InDone)));
        return lRequest;
    }

    OnlineRequestId OnlineServices::ClearAchievement(const OpaaxString& InName, FOnOnlineDone InDone)
    {
        const OnlineRequestId lRequest = Begin();
        IOnlineAchievements* lAchievements = m_Backend->GetAchievements();

        if (const EOnlineResult lCheck = CheckFeature(lAchievements); lCheck != EOnlineResult::Ok)
        {
            Fail(lRequest, InDone, lCheck);
            return lRequest;
        }

        lAchievements->Clear(InName, Relay(lRequest, Move(InDone)));
        return lRequest;
    }

    // =========================================================================
    // Overlay
    // =========================================================================
    bool OnlineServices::OpenOverlay(const EOverlayPage InPage, const OnlinePlayerId& InPlayer)
    {
        IOnlineOverlay* lOverlay = m_Backend->GetOverlay();
        return (lOverlay != nullptr) && lOverlay->Open(InPage, InPlayer);
    }
}
