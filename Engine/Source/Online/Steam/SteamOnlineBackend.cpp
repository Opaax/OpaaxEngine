#include "Online/Steam/SteamOnlineBackend.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IOnlineServices.h"
#include "Core/Algo/Algo.h"
#include "Core/String/OpaaxPathString.h"
#include "Online/Steam/SteamLibrary.h"
#include "Online/SteamRules.h"
#include "Platform/IPlatform.h"
#include "Platform/Process.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // SteamBackend — Steam once it started: the player, friends' names, the overlay. Every Steam
        //   callback comes through Pump (manual dispatch), on the main thread.
        // =====================================================================
        class SteamBackend final : public IOnlineBackend, public IOnlineIdentity, public IOnlineOverlay
        {
        public:
            SteamBackend(TUniquePtr<SteamLibrary> InSteam, ISteamUser* InUser, ISteamFriends* InFriends, ISteamUtils* InUtils)
                : m_Steam(Move(InSteam)), m_User(InUser), m_Friends(InFriends), m_Utils(InUtils)
            {
                m_Pipe = m_Steam->SteamAPI_GetHSteamPipe();
            }

            ~SteamBackend() override
            {
                Shutdown();
            }

            //~Begin IOnlineBackend interface
            EOnlineBackend GetType() const noexcept override { return EOnlineBackend::Steam; }

            OnlineCapabilities GetCapabilities() const override
            {
                OnlineCapabilities lCapabilities;
                lCapabilities.bIdentity = true;
                lCapabilities.bOverlay  = true;
                return lCapabilities;
            }

            void Pump() override
            {
                if (!m_bRunning)
                {
                    return;
                }

                m_Steam->SteamAPI_ManualDispatch_RunFrame(m_Pipe);

                CallbackMsg_t lMessage{};
                while (m_Steam->SteamAPI_ManualDispatch_GetNextCallback(m_Pipe, &lMessage))
                {
                    Dispatch(lMessage);
                    m_Steam->SteamAPI_ManualDispatch_FreeLastCallback(m_Pipe);
                }
            }

            void Shutdown() override
            {
                if (!m_bRunning)
                {
                    return;
                }

                m_bRunning = false;
                m_NameWaits.clear();
                m_Steam->SteamAPI_Shutdown();
                OPAAX_LOG(LogOnline, Info, "Steam shut down");
            }

            IOnlineIdentity* GetIdentity() override { return this; }
            IOnlineOverlay*  GetOverlay() override  { return this; }
            //~End IOnlineBackend interface

            //~Begin IOnlineIdentity interface
            OnlinePlayer GetLocalPlayer() const override
            {
                const Uint64 lId = m_Steam->SteamAPI_ISteamUser_GetSteamID(m_User);
                return OnlinePlayer{ OnlinePlayerId{ EOnlineBackend::Steam, SteamRules::IdText(lId) },
                                     OpaaxString(m_Steam->SteamAPI_ISteamFriends_GetPersonaName(m_Friends)) };
            }

            OpaaxString GetPlayerName(const OnlinePlayerId& InId) const override
            {
                const Uint64 lId = SteamRules::ParseId(InId.Value);
                if (lId == 0)
                {
                    return {};
                }

                const char* lName = m_Steam->SteamAPI_ISteamFriends_GetFriendPersonaName(m_Friends, lId);
                return SteamRules::IsKnownName(lName) ? OpaaxString(lName) : OpaaxString();
            }

            void RequestPlayerNames(const TDynArray<OnlinePlayerId>& InIds, FOnOnlineDone InDone) override
            {
                // Steam answers each name it must fetch with a PersonaStateChange; one it knows needs nothing.
                NameWait lWait;
                for (const OnlinePlayerId& lPlayer : InIds)
                {
                    const Uint64 lId = SteamRules::ParseId(lPlayer.Value);
                    if (lId != 0 && m_Steam->SteamAPI_ISteamFriends_RequestUserInformation(m_Friends, lId, true))
                    {
                        lWait.Waiting.push_back(lId);
                    }
                }

                if (lWait.Waiting.empty())
                {
                    InDone(EOnlineResult::Ok);
                    return;
                }

                lWait.Done = Move(InDone);
                m_NameWaits.push_back(Move(lWait));
            }

            TDynArray<OnlinePlayerId> GetFriends() const override
            {
                TDynArray<OnlinePlayerId> lFriends;
                const int lCount = m_Steam->SteamAPI_ISteamFriends_GetFriendCount(m_Friends, k_EFriendFlagImmediate);
                for (int lIndex = 0; lIndex < lCount; ++lIndex)
                {
                    const Uint64 lId = m_Steam->SteamAPI_ISteamFriends_GetFriendByIndex(m_Friends, lIndex, k_EFriendFlagImmediate);
                    lFriends.push_back(OnlinePlayerId{ EOnlineBackend::Steam, SteamRules::IdText(lId) });
                }
                return lFriends;
            }
            //~End IOnlineIdentity interface

            //~Begin IOnlineOverlay interface
            bool IsActive() const override { return m_bOverlayActive; }

            bool Open(const EOverlayPage InPage, const OnlinePlayerId& InPlayer) override
            {
                if (!m_Steam->SteamAPI_ISteamUtils_IsOverlayEnabled(m_Utils))
                {
                    return false;
                }

                if (InPage == EOverlayPage::PlayerProfile)
                {
                    const Uint64 lId = SteamRules::ParseId(InPlayer.Value);
                    if (lId == 0)
                    {
                        return false;
                    }
                    m_Steam->SteamAPI_ISteamFriends_ActivateGameOverlayToUser(m_Friends, SteamRules::OverlayDialog(InPage), lId);
                    return true;
                }

                m_Steam->SteamAPI_ISteamFriends_ActivateGameOverlay(m_Friends, SteamRules::OverlayDialog(InPage));
                return true;
            }
            //~End IOnlineOverlay interface

            /** The app Steam runs the game as. */
            Uint32 GetAppId() const { return m_Steam->SteamAPI_ISteamUtils_GetAppID(m_Utils); }

            /** False when Steam runs offline: the player is known, the store's servers are not reached. */
            bool IsLoggedOn() const { return m_Steam->SteamAPI_ISteamUser_BLoggedOn(m_User); }

        private:
            struct NameWait
            {
                TDynArray<Uint64> Waiting;
                FOnOnlineDone     Done;
            };

            void Dispatch(const CallbackMsg_t& InMessage)
            {
                switch (InMessage.m_iCallback)
                {
                    case GameOverlayActivated_t::k_iCallback:
                    {
                        const auto* lOverlay = reinterpret_cast<const GameOverlayActivated_t*>(InMessage.m_pubParam);
                        m_bOverlayActive = lOverlay->m_bActive != 0;
                        NotifyOverlayChanged(m_bOverlayActive);
                        break;
                    }

                    case PersonaStateChange_t::k_iCallback:
                    {
                        const auto* lChange = reinterpret_cast<const PersonaStateChange_t*>(InMessage.m_pubParam);
                        OnPersonaChanged(lChange->m_ulSteamID);
                        break;
                    }

                    default:
                        break;
                }
            }

            void OnPersonaChanged(const Uint64 InSteamId)
            {
                // The waits done now answer after the loop: an answer may ask for more names.
                TDynArray<FOnOnlineDone> lDone;
                for (auto lIt = m_NameWaits.begin(); lIt != m_NameWaits.end();)
                {
                    Algo::RemoveIf(lIt->Waiting, [InSteamId](const Uint64 InWaiting) { return InWaiting == InSteamId; });
                    if (lIt->Waiting.empty())
                    {
                        lDone.push_back(Move(lIt->Done));
                        lIt = m_NameWaits.erase(lIt);
                    }
                    else
                    {
                        ++lIt;
                    }
                }

                for (const FOnOnlineDone& lAnswer : lDone)
                {
                    lAnswer(EOnlineResult::Ok);
                }
            }

            TUniquePtr<SteamLibrary> m_Steam;
            ISteamUser*              m_User    = nullptr;
            ISteamFriends*           m_Friends = nullptr;
            ISteamUtils*             m_Utils   = nullptr;
            HSteamPipe               m_Pipe    = 0;

            bool                m_bRunning       = true;
            bool                m_bOverlayActive = false;
            TDynArray<NameWait> m_NameWaits;
        };
    }

    TUniquePtr<IOnlineBackend> SteamOnlineBackend::Start(const SteamOnlineSettings& InSettings, OpaaxString& OutReason)
    {
        // steam_api beside the executable, by its full path: never one found elsewhere on the machine.
        const OpaaxString lFolder = PathString::Parent(OpaaxApplication::GetAppService<IPlatform>().GetExecutablePath().View());
        const OpaaxString lPath   = PathString::Join(lFolder.View(), SteamLibrary::GetFileName());

        TUniquePtr<SteamLibrary> lSteam = MakeUnique<SteamLibrary>();
        if (!lSteam->Load(lPath, OutReason))
        {
            return nullptr;
        }

        // Started outside Steam, the game names its app; started by Steam, Steam already has.
        if (SteamRules::ShouldSetAppId(InSettings.AppId, Process::GetEnvironment("SteamAppId").has_value()))
        {
            const OpaaxString lAppId = OpaaxString::Format("{}", InSettings.AppId);
            Process::SetEnvironment("SteamAppId", lAppId);
            Process::SetEnvironment("SteamGameId", lAppId);
        }

        SteamErrMsg lMessage{};
        const ESteamAPIInitResult lResult = lSteam->SteamAPI_InitFlat(&lMessage);
        if (lResult != k_ESteamAPIInitResult_OK)
        {
            OutReason = SteamRules::InitFailure(static_cast<Int32>(lResult), lMessage);
            return nullptr;
        }

        lSteam->SteamAPI_ManualDispatch_Init();

        ISteamUser*    lUser    = lSteam->SteamAPI_SteamUser_v023();
        ISteamFriends* lFriends = lSteam->SteamAPI_SteamFriends_v018();
        ISteamUtils*   lUtils   = lSteam->SteamAPI_SteamUtils_v011();
        if (lUser == nullptr || lFriends == nullptr || lUtils == nullptr)
        {
            lSteam->SteamAPI_Shutdown();
            OutReason = "Steam started without its user, friends or utils interface";
            return nullptr;
        }

        TUniquePtr<SteamBackend> lBackend = MakeUnique<SteamBackend>(Move(lSteam), lUser, lFriends, lUtils);
        const OnlinePlayer lPlayer = lBackend->GetLocalPlayer();
        OPAAX_LOG(LogOnline, Info, "Steam started: app {}, player {} ({}){}", lBackend->GetAppId(), lPlayer.Name.CStr(),
                  lPlayer.Id.Value.CStr(), lBackend->IsLoggedOn() ? "" : ", offline");
        return lBackend;
    }
}
