#pragma once

#include "Application/Services/IOnlineServices.h"
#include "Core/Events/Delegate.h"
#include "Core/Events/DelegateHandle.h"
#include "Core/OpaaxTypes.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"

namespace Opaax
{
    // =============================================================================
    // OnlineSubsystem — a game's way to the player's store (IOnlineServices): the same calls, for the
    //   game's lifetime. What the game asked and is not answered when it ends (Play in Editor stopped,
    //   game quit) is dropped, so no callback runs into a game that is gone. It lasts across levels: a
    //   behaviour that asks guards itself in its callback (TWeakObjectPtr), as for a save.
    // =============================================================================
    class OnlineSubsystem final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(OnlineSubsystem, GameInstanceSubsystem)

        // =============================================================================
        // Store
        // =============================================================================
    public:
        bool               IsAvailable() const;
        OnlineCapabilities GetCapabilities() const;
        OnlinePlayer       GetLocalPlayer() const;
        OpaaxString        GetPlayerName(const OnlinePlayerId& InId) const;

        // =============================================================================
        // Requests — see IOnlineServices; each answers at a later frame, or never once the game ended
        // =============================================================================
    public:
        OnlineRequestId UploadScore(OpaaxStringView InBoard, Int32 InScore, const TDynArray<Int32>& InDetails,
                                    const TDynArray<Uint8>& InFile, FOnScoreUploaded InDone);
        OnlineRequestId DownloadEntries(OpaaxStringView InBoard, ELeaderboardRange InRange, Int32 InFirst, Int32 InLast,
                                        FOnEntries InDone);
        OnlineRequestId DownloadFile(const OnlineFileId& InFile, FOnFile InDone);
        OnlineRequestId RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone);

        OnlineRequestId UnlockAchievement(const OpaaxString& InName, FOnOnlineDone InDone = {});
        OnlineRequestId IndicateAchievementProgress(const OpaaxString& InName, Uint32 InCurrent, Uint32 InMax,
                                                    FOnOnlineDone InDone = {});
        OnlineRequestId GetAchievements(FOnAchievements InDone);

        /** InRequest's callback never runs. */
        void Cancel(OnlineRequestId InRequest);

        /** The game's requests not answered yet. */
        Uint32 GetPendingCount() const;

        // =============================================================================
        // Overlay
        // =============================================================================
    public:
        bool IsOverlayActive() const;
        bool OpenOverlay(EOverlayPage InPage, const OnlinePlayerId& InPlayer = {});

        /** Broadcast when the store's overlay opens (true) and closes: pause the game there. */
        TMulticastDelegate<bool>& OnOverlayChanged() noexcept { return m_OnOverlayChanged; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Subsystem interface
    public:
        bool Startup() override;
        void Shutdown() override;
        //~End Subsystem interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** The services: the game's context's, or the null one before Startup. */
        IOnlineServices& Online() const;

        /** Keeps InRequest to drop it at Shutdown, and forgets those already answered. */
        OnlineRequestId Track(OnlineRequestId InRequest);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IOnlineServices*          m_Online = nullptr;
        TDynArray<OnlineRequestId> m_Requests;
        TMulticastDelegate<bool>  m_OnOverlayChanged;
        DelegateHandle            m_OverlayHandle;
    };
}
