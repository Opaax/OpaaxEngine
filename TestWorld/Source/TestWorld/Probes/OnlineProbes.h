#pragma once

#include "Core/Events/DelegateHandle.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Object/ObjectPtr.h"
#include "Online/OnlineSubsystem.h"
#include "World/Behaviour/Behaviour.h"
#include "World/WorldManager.h"

namespace TestWorld
{
    // =============================================================================
    // LeaderboardProbe — a results screen: when it starts, uploads Score to Board with Ghost attached
    //   (when the store keeps files), then reads the lines around the player and their own ghost back,
    //   as a game shows a run's result. Each answer goes in its fields, where the test reads them.
    // =============================================================================
    class LeaderboardProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(LeaderboardProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Board;
        Opaax::Int32       Score = 0;
        Opaax::OpaaxString Ghost;

        /** The store keeps files: the ghost went with the score. */
        bool bGhostSent = false;

        Opaax::OpaaxString UploadAnswer;
        bool               bNewBest = false;
        Opaax::Int32       Rank     = 0;

        /** The lines around the player, "1. Ada 1000 | 2. Player 1200". */
        Opaax::OpaaxString EntriesAnswer;
        Opaax::OpaaxString Lines;

        /** The player's own ghost, read back from their entry; empty without one. */
        Opaax::OpaaxString GhostBack;

        /** Set when an answer came inside the call that asked for it: it must come at a later frame. */
        bool bAnsweredInCall = false;

        OPAAX_PROPERTIES(LeaderboardProbe, OPAAX_PROP(Board), OPAAX_PROP(Score), OPAAX_PROP(Ghost), OPAAX_PROP(bGhostSent),
                         OPAAX_PROP(UploadAnswer), OPAAX_PROP(bNewBest), OPAAX_PROP(Rank), OPAAX_PROP(EntriesAnswer),
                         OPAAX_PROP(Lines), OPAAX_PROP(GhostBack), OPAAX_PROP(bAnsweredInCall))

        void OnStart() override
        {
            Opaax::OnlineSubsystem* lOnline = GetGameSubsystem<Opaax::OnlineSubsystem>();
            if (lOnline == nullptr)
            {
                UploadAnswer = "NoOnlineSubsystem";
                return;
            }

            bGhostSent = lOnline->GetCapabilities().bLeaderboardFiles && !Ghost.IsEmpty();
            Opaax::TDynArray<Opaax::Uint8> lGhost;
            if (bGhostSent)
            {
                const Opaax::Uint8* lBytes = reinterpret_cast<const Opaax::Uint8*>(Ghost.CStr());
                lGhost.assign(lBytes, lBytes + Ghost.GetLength());
            }

            // The answers come frames later: this probe may be gone by then.
            const Opaax::TWeakObjectPtr<LeaderboardProbe> lSelf(this);

            m_bAsking = true;
            lOnline->UploadScore(Board.View(), Score, {}, lGhost,
                [lSelf, lOnline](const Opaax::EOnlineResult InResult, const Opaax::ScoreUploadResult& InUpload)
                {
                    if (LeaderboardProbe* lProbe = lSelf.Get())
                    {
                        lProbe->OnUploaded(*lOnline, InResult, InUpload);
                    }
                });
            m_bAsking = false;
        }

    private:
        void OnUploaded(Opaax::OnlineSubsystem& InOnline, const Opaax::EOnlineResult InResult,
                        const Opaax::ScoreUploadResult& InUpload)
        {
            bAnsweredInCall = bAnsweredInCall || m_bAsking;
            UploadAnswer    = Opaax::ToString(InResult);
            bNewBest        = InUpload.bScoreChanged;
            Rank            = InUpload.Rank;

            const Opaax::TWeakObjectPtr<LeaderboardProbe> lSelf(this);
            InOnline.DownloadEntries(Board.View(), Opaax::ELeaderboardRange::AroundPlayer, -1, 1,
                [lSelf, &InOnline](const Opaax::EOnlineResult InEntriesResult, const Opaax::TDynArray<Opaax::LeaderboardEntry>& InEntries)
                {
                    if (LeaderboardProbe* lProbe = lSelf.Get())
                    {
                        lProbe->OnEntries(InOnline, InEntriesResult, InEntries);
                    }
                });
        }

        void OnEntries(Opaax::OnlineSubsystem& InOnline, const Opaax::EOnlineResult InResult,
                       const Opaax::TDynArray<Opaax::LeaderboardEntry>& InEntries)
        {
            EntriesAnswer = Opaax::ToString(InResult);

            Opaax::OnlineFileId lMine;
            for (const Opaax::LeaderboardEntry& lEntry : InEntries)
            {
                Lines += Lines.IsEmpty() ? "" : " | ";
                Lines += Opaax::OpaaxString::Format("{}. {} {}", lEntry.Rank, lEntry.Player.Name.CStr(), lEntry.Score);
                if (lEntry.Player.Id == InOnline.GetLocalPlayer().Id)
                {
                    lMine = lEntry.File;
                }
            }

            if (!lMine.IsValid())
            {
                return;
            }

            const Opaax::TWeakObjectPtr<LeaderboardProbe> lSelf(this);
            InOnline.DownloadFile(lMine, [lSelf](Opaax::EOnlineResult, const Opaax::TDynArray<Opaax::Uint8>& InBytes)
            {
                if (LeaderboardProbe* lProbe = lSelf.Get())
                {
                    lProbe->GhostBack = Opaax::OpaaxString(reinterpret_cast<const char*>(InBytes.data()),
                                                           static_cast<Opaax::Uint32>(InBytes.size()));
                }
            });
        }

        bool m_bAsking = false;
    };

    // =============================================================================
    // OverlayPauseProbe — pauses the game while the store's overlay is open, as a game must: the
    //   player is looking at Steam, not the game. Counts the overlay's changes.
    // =============================================================================
    class OverlayPauseProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(OverlayPauseProbe, Opaax::Behaviour)

    public:
        bool         bOverlayOpen = false;
        Opaax::Int32 Changes      = 0;

        OPAAX_PROPERTIES(OverlayPauseProbe, OPAAX_PROP(bOverlayOpen), OPAAX_PROP(Changes))

        void OnStart() override
        {
            m_Online = GetGameSubsystem<Opaax::OnlineSubsystem>();
            if (m_Online == nullptr)
            {
                return;
            }

            // The overlay changes outside the world's tick, paused or not.
            const Opaax::TWeakObjectPtr<OverlayPauseProbe> lSelf(this);
            m_Handle = m_Online->OnOverlayChanged().Add([lSelf](const bool bInActive)
            {
                if (OverlayPauseProbe* lProbe = lSelf.Get())
                {
                    lProbe->OnOverlay(bInActive);
                }
            });
        }

        void OnDestroy() override
        {
            if (m_Online != nullptr)
            {
                m_Online->OnOverlayChanged().Remove(m_Handle);
            }
        }

    private:
        void OnOverlay(const bool bInActive)
        {
            bOverlayOpen = bInActive;
            ++Changes;
            m_Online->GetContext().Worlds.SetPaused(bInActive);
        }

        Opaax::OnlineSubsystem* m_Online = nullptr;
        Opaax::DelegateHandle   m_Handle;
    };
}
