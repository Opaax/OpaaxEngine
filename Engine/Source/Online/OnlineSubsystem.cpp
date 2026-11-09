#include "Online/OnlineSubsystem.h"

#include "Core/Algo/Algo.h"
#include "Engine/GameInstance/GameInstanceContext.h"

namespace Opaax
{
    // =========================================================================
    // Lifetime
    // =========================================================================
    bool OnlineSubsystem::Startup()
    {
        m_Online = &GetContext().Online;

        // The game hears the overlay through its own delegate: its listeners go with it.
        m_OverlayHandle = m_Online->OnOverlayChanged().Add([this](const bool bInActive)
        {
            m_OnOverlayChanged.Broadcast(bInActive);
        });
        return true;
    }

    void OnlineSubsystem::Shutdown()
    {
        if (m_Online != nullptr)
        {
            for (const OnlineRequestId lRequest : m_Requests)
            {
                m_Online->Cancel(lRequest);
            }
            m_Online->OnOverlayChanged().Remove(m_OverlayHandle);
        }

        m_Requests.clear();
        m_OnOverlayChanged.Clear();
        m_Online = nullptr;
    }

    IOnlineServices& OnlineSubsystem::Online() const
    {
        return (m_Online != nullptr) ? *m_Online : IOnlineServices::Null();
    }

    OnlineRequestId OnlineSubsystem::Track(const OnlineRequestId InRequest)
    {
        IOnlineServices& lOnline = Online();
        Algo::RemoveIf(m_Requests, [&lOnline](const OnlineRequestId InKept) { return !lOnline.IsPending(InKept); });

        if (InRequest != INVALID_ONLINE_REQUEST)
        {
            m_Requests.push_back(InRequest);
        }
        return InRequest;
    }

    // =========================================================================
    // Store
    // =========================================================================
    bool               OnlineSubsystem::IsAvailable() const     { return Online().IsAvailable(); }
    OnlineCapabilities OnlineSubsystem::GetCapabilities() const { return Online().GetCapabilities(); }
    OnlinePlayer       OnlineSubsystem::GetLocalPlayer() const  { return Online().GetLocalPlayer(); }

    OpaaxString OnlineSubsystem::GetPlayerName(const OnlinePlayerId& InId) const
    {
        return Online().GetPlayerName(InId);
    }

    // =========================================================================
    // Requests
    // =========================================================================
    OnlineRequestId OnlineSubsystem::UploadScore(const OpaaxStringView InBoard, const Int32 InScore,
                                                 const TDynArray<Int32>& InDetails, const TDynArray<Uint8>& InFile,
                                                 FOnScoreUploaded InDone)
    {
        return Track(Online().UploadScore(InBoard, InScore, InDetails, InFile, Move(InDone)));
    }

    OnlineRequestId OnlineSubsystem::DownloadEntries(const OpaaxStringView InBoard, const ELeaderboardRange InRange,
                                                     const Int32 InFirst, const Int32 InLast, FOnEntries InDone)
    {
        return Track(Online().DownloadEntries(InBoard, InRange, InFirst, InLast, Move(InDone)));
    }

    OnlineRequestId OnlineSubsystem::DownloadFile(const OnlineFileId& InFile, FOnFile InDone)
    {
        return Track(Online().DownloadFile(InFile, Move(InDone)));
    }

    OnlineRequestId OnlineSubsystem::RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone)
    {
        return Track(Online().RequestPlayerNames(InIds, Move(InDone)));
    }

    OnlineRequestId OnlineSubsystem::UnlockAchievement(const OpaaxString& InName, FOnOnlineDone InDone)
    {
        return Track(Online().UnlockAchievement(InName, Move(InDone)));
    }

    OnlineRequestId OnlineSubsystem::IndicateAchievementProgress(const OpaaxString& InName, const Uint32 InCurrent,
                                                                 const Uint32 InMax, FOnOnlineDone InDone)
    {
        return Track(Online().IndicateAchievementProgress(InName, InCurrent, InMax, Move(InDone)));
    }

    OnlineRequestId OnlineSubsystem::GetAchievements(FOnAchievements InDone)
    {
        return Track(Online().GetAchievements(Move(InDone)));
    }

    void OnlineSubsystem::Cancel(const OnlineRequestId InRequest)
    {
        Online().Cancel(InRequest);
    }

    Uint32 OnlineSubsystem::GetPendingCount() const
    {
        IOnlineServices& lOnline = Online();
        return static_cast<Uint32>(Algo::CountIf(m_Requests, [&lOnline](const OnlineRequestId InRequest)
        {
            return lOnline.IsPending(InRequest);
        }));
    }

    // =========================================================================
    // Overlay
    // =========================================================================
    bool OnlineSubsystem::IsOverlayActive() const
    {
        return Online().IsOverlayActive();
    }

    bool OnlineSubsystem::OpenOverlay(const EOverlayPage InPage, const OnlinePlayerId& InPlayer)
    {
        return Online().OpenOverlay(InPage, InPlayer);
    }
}
