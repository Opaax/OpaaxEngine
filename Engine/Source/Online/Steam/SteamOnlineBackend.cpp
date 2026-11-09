#include "Online/Steam/SteamOnlineBackend.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IOnlineServices.h"
#include "Core/Algo/Algo.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxPathString.h"
#include "Online/Steam/SteamLibrary.h"
#include "Online/SteamRules.h"
#include "Platform/IPlatform.h"
#include "Platform/Process.h"

namespace Opaax
{
    namespace
    {
        // The rules' numbers are Steam's: a change in the SDK fails here.
        static_assert(SteamRules::SortMethod(ELeaderboardSort::Ascending) == k_ELeaderboardSortMethodAscending);
        static_assert(SteamRules::SortMethod(ELeaderboardSort::Descending) == k_ELeaderboardSortMethodDescending);
        static_assert(SteamRules::DisplayType(ELeaderboardDisplay::Numeric) == k_ELeaderboardDisplayTypeNumeric);
        static_assert(SteamRules::DisplayType(ELeaderboardDisplay::TimeSeconds) == k_ELeaderboardDisplayTypeTimeSeconds);
        static_assert(SteamRules::DisplayType(ELeaderboardDisplay::TimeMilliseconds) == k_ELeaderboardDisplayTypeTimeMilliSeconds);
        static_assert(SteamRules::DataRequest(ELeaderboardRange::World) == k_ELeaderboardDataRequestGlobal);
        static_assert(SteamRules::DataRequest(ELeaderboardRange::AroundPlayer) == k_ELeaderboardDataRequestGlobalAroundUser);
        static_assert(SteamRules::DataRequest(ELeaderboardRange::Friends) == k_ELeaderboardDataRequestFriends);
        static_assert(SteamRules::MAX_DETAILS == static_cast<Uint32>(k_cLeaderboardDetailsMax));
        static_assert(SteamRules::BOARD_NAME_SIZE == static_cast<Uint64>(k_cchLeaderboardNameMax));
        static_assert(SteamRules::MAX_FILE_BYTES == static_cast<Uint64>(k_unMaxCloudFileChunkSize));
        static_assert(SteamRules::FILE_NAME_SIZE == static_cast<Uint64>(k_cchFilenameMax));
        static_assert(SteamRules::FileResult(k_EResultOK) == EOnlineResult::Ok);
        static_assert(SteamRules::FileResult(k_EResultFail) == EOnlineResult::Failed);
        static_assert(SteamRules::FileResult(k_EResultFileNotFound) == EOnlineResult::NotFound);
        static_assert(SteamRules::FileResult(k_EResultTimeout) == EOnlineResult::TimedOut);
        static_assert(SteamRules::FileResult(k_EResultNotLoggedOn) == EOnlineResult::NotSignedIn);
        static_assert(SteamRules::FileResult(k_EResultLimitExceeded) == EOnlineResult::QuotaExceeded);
        static_assert(SteamRules::FileResult(k_EResultDiskFull) == EOnlineResult::QuotaExceeded);

        // =====================================================================
        // SteamBackend — Steam once it started: the player, friends' names, leaderboards and the files
        //   on their entries, the overlay. Every Steam callback and call result comes through Pump
        //   (manual dispatch), on the main thread.
        // =====================================================================
        class SteamBackend final : public IOnlineBackend, public IOnlineIdentity, public IOnlineLeaderboards, public IOnlineFiles,
                                   public IOnlineOverlay
        {
        public:
            SteamBackend(TUniquePtr<SteamLibrary> InSteam, ISteamUser* InUser, ISteamFriends* InFriends, ISteamUtils* InUtils,
                         ISteamUserStats* InStats, ISteamRemoteStorage* InStorage)
                : m_Steam(Move(InSteam)), m_User(InUser), m_Friends(InFriends), m_Utils(InUtils), m_Stats(InStats),
                  m_Storage(InStorage)
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

                if (m_Stats != nullptr)
                {
                    lCapabilities.bLeaderboards              = true;
                    lCapabilities.bLeaderboardsMadeAtRunTime = true;
                    lCapabilities.bFriendsRange              = true;
                    lCapabilities.MaxDetails                 = SteamRules::MAX_DETAILS;
                    lCapabilities.MaxFileBytes               = GetMaxFileBytes();
                    lCapabilities.bLeaderboardFiles          = lCapabilities.MaxFileBytes > 0;
                }
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
                m_CallWaits.clear();
                m_FilesInFlight.clear();
                m_Steam->SteamAPI_Shutdown();
                OPAAX_LOG(LogOnline, Info, "Steam shut down");
            }

            IOnlineIdentity*     GetIdentity() override     { return this; }
            IOnlineLeaderboards* GetLeaderboards() override { return m_Stats != nullptr ? this : nullptr; }
            IOnlineFiles*        GetFiles() override        { return GetMaxFileBytes() > 0 ? this : nullptr; }
            IOnlineOverlay*      GetOverlay() override      { return this; }
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

            //~Begin IOnlineLeaderboards interface
            void FindLeaderboard(const LeaderboardDefinition& InDefinition, FOnLeaderboardFound InDone) override
            {
                if (!IsLoggedOn())
                {
                    InDone(EOnlineResult::NotSignedIn, {});
                    return;
                }

                if (!SteamRules::IsValidBoardName(InDefinition.Name))
                {
                    OPAAX_LOG(LogOnline, Warn, "Steam takes no leaderboard named \"{}\": 1 to {} characters", InDefinition.Name.CStr(),
                              SteamRules::BOARD_NAME_SIZE - 1);
                    InDone(EOnlineResult::Failed, {});
                    return;
                }

                const SteamAPICall_t lCall = m_Steam->SteamAPI_ISteamUserStats_FindOrCreateLeaderboard(
                    m_Stats, InDefinition.Name.CStr(),
                    static_cast<ELeaderboardSortMethod>(SteamRules::SortMethod(InDefinition.Sort)),
                    static_cast<ELeaderboardDisplayType>(SteamRules::DisplayType(InDefinition.Display)));

                Await<LeaderboardFindResult_t>(lCall, [this, InDefinition, InDone = Move(InDone)](const LeaderboardFindResult_t* InResult)
                {
                    if (InResult == nullptr)
                    {
                        InDone(EOnlineResult::Failed, {});
                        return;
                    }

                    if (InResult->m_bLeaderboardFound == 0 || InResult->m_hSteamLeaderboard == 0)
                    {
                        InDone(EOnlineResult::NotFound, {});
                        return;
                    }

                    // A board made earlier keeps its sort on Steam, whatever the game says now.
                    const TOptional<ELeaderboardSort> lSteamSort = SteamRules::SortFromMethod(
                        m_Steam->SteamAPI_ISteamUserStats_GetLeaderboardSortMethod(m_Stats, InResult->m_hSteamLeaderboard));
                    if (lSteamSort.has_value() && *lSteamSort != InDefinition.Sort)
                    {
                        OPAAX_LOG(LogOnline, Warn, "Leaderboard {} sorts {} on Steam but {} in the game: Steam keeps its own",
                                  InDefinition.Name.CStr(), ToString(*lSteamSort), ToString(InDefinition.Sort));
                    }

                    InDone(EOnlineResult::Ok, OnlineLeaderboardHandle{ SteamRules::IdText(InResult->m_hSteamLeaderboard) });
                });
            }

            void UploadScore(const OnlineLeaderboardHandle& InBoard, const Int32 InScore, const TDynArray<Int32>& InDetails,
                             FOnScoreUploaded InDone) override
            {
                const SteamLeaderboard_t lBoard = SteamRules::ParseId(InBoard.Value);
                if (lBoard == 0)
                {
                    InDone(EOnlineResult::NotFound, {});
                    return;
                }
                if (!IsLoggedOn())
                {
                    InDone(EOnlineResult::NotSignedIn, {});
                    return;
                }

                // The services cut the details to MaxDetails already.
                const int lDetailCount = static_cast<int>(Maths::Min<Uint64>(InDetails.size(), SteamRules::MAX_DETAILS));
                const SteamAPICall_t lCall = m_Steam->SteamAPI_ISteamUserStats_UploadLeaderboardScore(
                    m_Stats, lBoard, k_ELeaderboardUploadScoreMethodKeepBest, InScore,
                    lDetailCount > 0 ? InDetails.data() : nullptr, lDetailCount);

                Await<LeaderboardScoreUploaded_t>(lCall, [InDone = Move(InDone)](const LeaderboardScoreUploaded_t* InResult)
                {
                    if (InResult == nullptr || InResult->m_bSuccess == 0)
                    {
                        InDone(EOnlineResult::Failed, {});
                        return;
                    }

                    ScoreUploadResult lUpload;
                    lUpload.bScoreChanged = InResult->m_bScoreChanged != 0;
                    lUpload.Rank          = InResult->m_nGlobalRankNew;
                    lUpload.PreviousRank  = InResult->m_nGlobalRankPrevious;
                    InDone(EOnlineResult::Ok, lUpload);
                });
            }

            void DownloadEntries(const OnlineLeaderboardHandle& InBoard, const ELeaderboardRange InRange, const Int32 InFirst,
                                 const Int32 InLast, FOnEntries InDone) override
            {
                const SteamLeaderboard_t lBoard = SteamRules::ParseId(InBoard.Value);
                if (lBoard == 0)
                {
                    InDone(EOnlineResult::NotFound, {});
                    return;
                }
                if (!IsLoggedOn())
                {
                    InDone(EOnlineResult::NotSignedIn, {});
                    return;
                }

                const SteamAPICall_t lCall = m_Steam->SteamAPI_ISteamUserStats_DownloadLeaderboardEntries(
                    m_Stats, lBoard, static_cast<ELeaderboardDataRequest>(SteamRules::DataRequest(InRange)), InFirst, InLast);
                AwaitEntries(lCall, Move(InDone));
            }

            void DownloadEntriesForPlayers(const OnlineLeaderboardHandle& InBoard, const TDynArray<OnlinePlayerId>& InPlayers,
                                           FOnEntries InDone) override
            {
                const SteamLeaderboard_t lBoard = SteamRules::ParseId(InBoard.Value);
                if (lBoard == 0)
                {
                    InDone(EOnlineResult::NotFound, {});
                    return;
                }
                if (!IsLoggedOn())
                {
                    InDone(EOnlineResult::NotSignedIn, {});
                    return;
                }

                TDynArray<CSteamID> lUsers;
                for (const OnlinePlayerId& lPlayer : InPlayers)
                {
                    const Uint64 lId = SteamRules::ParseId(lPlayer.Value);
                    if (lId != 0)
                    {
                        lUsers.push_back(CSteamID(lId));
                    }
                }

                if (lUsers.empty())
                {
                    InDone(EOnlineResult::Ok, {});
                    return;
                }

                if (lUsers.size() > SteamRules::MAX_PLAYERS_PER_DOWNLOAD)
                {
                    OPAAX_LOG(LogOnline, Warn, "Steam gives the entries of {} players at a time: {} left out",
                              SteamRules::MAX_PLAYERS_PER_DOWNLOAD, lUsers.size() - SteamRules::MAX_PLAYERS_PER_DOWNLOAD);
                    lUsers.resize(SteamRules::MAX_PLAYERS_PER_DOWNLOAD);
                }

                const SteamAPICall_t lCall = m_Steam->SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers(
                    m_Stats, lBoard, lUsers.data(), static_cast<int>(lUsers.size()));
                AwaitEntries(lCall, Move(InDone));
            }
            //~End IOnlineLeaderboards interface

            //~Begin IOnlineFiles interface
            void AttachFile(const OnlineLeaderboardHandle& InBoard, const TDynArray<Uint8>& InBytes, FOnOnlineDone InDone) override
            {
                const SteamLeaderboard_t lBoard = SteamRules::ParseId(InBoard.Value);
                if (lBoard == 0)
                {
                    InDone(EOnlineResult::NotFound);
                    return;
                }
                if (!IsLoggedOn())
                {
                    InDone(EOnlineResult::NotSignedIn);
                    return;
                }

                const char* lBoardName = m_Steam->SteamAPI_ISteamUserStats_GetLeaderboardName(m_Stats, lBoard);
                const OpaaxString lFile = SteamRules::BoardFileName(OpaaxString(lBoardName != nullptr ? lBoardName : ""));
                if (lFile.IsEmpty())
                {
                    OPAAX_LOG(LogOnline, Warn, "No Steam Cloud file name for leaderboard \"{}\": its name is too long once escaped",
                              lBoardName != nullptr ? lBoardName : "");
                    InDone(EOnlineResult::Failed);
                    return;
                }

                // One file per board: a second write while the first is shared would attach either.
                if (Algo::Contains(m_FilesInFlight, lFile))
                {
                    OPAAX_LOG(LogOnline, Warn, "{} is still being attached: wait for its answer before the next file", lFile.CStr());
                    InDone(EOnlineResult::Failed);
                    return;
                }

                uint64 lTotal = 0;
                uint64 lAvailable = 0;
                m_Steam->SteamAPI_ISteamRemoteStorage_GetQuota(m_Storage, &lTotal, &lAvailable);
                const Uint64 lReplaced = static_cast<Uint64>(
                    Maths::Max<int32>(m_Steam->SteamAPI_ISteamRemoteStorage_GetFileSize(m_Storage, lFile.CStr()), 0));

                const EOnlineResult lFits = SteamRules::CheckFileSize(InBytes.size(), GetMaxFileBytes(), lAvailable, lReplaced);
                if (lFits != EOnlineResult::Ok)
                {
                    OPAAX_LOG(LogOnline, Warn, "{} ({} bytes) does not fit the player's Steam Cloud: {} bytes free, files of {} at most",
                              lFile.CStr(), InBytes.size(), lAvailable + lReplaced, GetMaxFileBytes());
                    InDone(lFits);
                    return;
                }

                // Shared, so the bytes Steam was given stay where they are until it has written them.
                const TSharedPtr<TDynArray<Uint8>> lBytes = MakeShared<TDynArray<Uint8>>(InBytes);
                m_FilesInFlight.push_back(lFile);

                const SteamAPICall_t lCall = m_Steam->SteamAPI_ISteamRemoteStorage_FileWriteAsync(
                    m_Storage, lFile.CStr(), lBytes->data(), static_cast<uint32>(lBytes->size()));

                Await<RemoteStorageFileWriteAsyncComplete_t>(lCall,
                    [this, lBoard, lFile, lBytes, InDone = Move(InDone)](const RemoteStorageFileWriteAsyncComplete_t* InWritten)
                    {
                        if (InWritten == nullptr || InWritten->m_eResult != k_EResultOK)
                        {
                            FinishAttach(lFile, lBytes->size(), "written", InWritten != nullptr ? InWritten->m_eResult : k_EResultFail, InDone);
                            return;
                        }
                        ShareAndAttach(lBoard, lFile, lBytes->size(), InDone);
                    });
            }

            void DownloadFile(const OnlineFileId& InFile, FOnFile InDone) override
            {
                const UGCHandle_t lHandle = SteamRules::ParseId(InFile.Value);
                if (lHandle == 0 || lHandle == k_UGCHandleInvalid)
                {
                    InDone(EOnlineResult::NotFound, {});
                    return;
                }
                if (!IsLoggedOn())
                {
                    InDone(EOnlineResult::NotSignedIn, {});
                    return;
                }

                const SteamAPICall_t lCall = m_Steam->SteamAPI_ISteamRemoteStorage_UGCDownload(m_Storage, lHandle, 0);

                Await<RemoteStorageDownloadUGCResult_t>(lCall,
                    [this, lHandle, InDone = Move(InDone)](const RemoteStorageDownloadUGCResult_t* InDownloaded)
                    {
                        if (InDownloaded == nullptr || InDownloaded->m_eResult != k_EResultOK)
                        {
                            const Int32 lSteamResult = InDownloaded != nullptr ? InDownloaded->m_eResult : k_EResultFail;
                            OPAAX_LOG(LogOnline, Warn, "Steam file {} not downloaded: Steam result {}", lHandle, lSteamResult);
                            InDone(SteamRules::FileResult(lSteamResult), {});
                            return;
                        }

                        const int32 lSize = InDownloaded->m_nSizeInBytes;
                        if (lSize < 0 || static_cast<Uint64>(lSize) > SteamRules::MAX_FILE_BYTES)
                        {
                            OPAAX_LOG(LogOnline, Warn, "Steam file {} is {} bytes: not a file of a leaderboard entry", lHandle, lSize);
                            InDone(EOnlineResult::Failed, {});
                            return;
                        }

                        TDynArray<Uint8> lBytes(static_cast<Uint64>(lSize));
                        if (lSize > 0)
                        {
                            const int32 lRead = m_Steam->SteamAPI_ISteamRemoteStorage_UGCRead(
                                m_Storage, lHandle, lBytes.data(), lSize, 0, k_EUGCRead_ContinueReadingUntilFinished);
                            if (lRead != lSize)
                            {
                                OPAAX_LOG(LogOnline, Warn, "Steam file {}: {} of {} bytes read", lHandle, lRead, lSize);
                                InDone(EOnlineResult::Failed, {});
                                return;
                            }
                        }

                        OPAAX_LOG(LogOnline, Info, "Steam file {} downloaded: {} bytes", lHandle, lSize);
                        InDone(EOnlineResult::Ok, lBytes);
                    });
            }
            //~End IOnlineFiles interface

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

            /** A Steam call waiting for its result; Finish reads it and answers. */
            struct CallWait
            {
                SteamAPICall_t   Call = k_uAPICallInvalid;
                TFunction<void()> Finish;
            };

            /**
             * Answers InDone with InCall's result once Steam has it, or with null when the call failed (an
             * invalid call fails at once).
             */
            template <typename TResult>
            void Await(const SteamAPICall_t InCall, TFunction<void(const TResult*)> InDone)
            {
                if (InCall == k_uAPICallInvalid)
                {
                    InDone(nullptr);
                    return;
                }

                m_CallWaits.push_back(CallWait{ InCall, [this, InCall, InDone = Move(InDone)]()
                {
                    TResult lResult{};
                    bool lbFailed = true;
                    const bool lbRead = m_Steam->SteamAPI_ManualDispatch_GetAPICallResult(
                        m_Pipe, InCall, &lResult, static_cast<int>(sizeof(TResult)), TResult::k_iCallback, &lbFailed);
                    InDone((lbRead && !lbFailed) ? &lResult : nullptr);
                } });
            }

            /** Answers InDone with the entries InCall downloads, read while Steam still holds them. */
            void AwaitEntries(const SteamAPICall_t InCall, FOnEntries InDone)
            {
                Await<LeaderboardScoresDownloaded_t>(InCall, [this, InDone = Move(InDone)](const LeaderboardScoresDownloaded_t* InResult)
                {
                    if (InResult == nullptr)
                    {
                        InDone(EOnlineResult::Failed, {});
                        return;
                    }

                    TDynArray<LeaderboardEntry> lEntries;
                    lEntries.reserve(static_cast<Uint64>(Maths::Max(InResult->m_cEntryCount, 0)));

                    TFixedArray<int32, SteamRules::MAX_DETAILS> lDetails{};
                    for (int lIndex = 0; lIndex < InResult->m_cEntryCount; ++lIndex)
                    {
                        LeaderboardEntry_t lSteamEntry{};
                        if (!m_Steam->SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry(
                                m_Stats, InResult->m_hSteamLeaderboardEntries, lIndex, &lSteamEntry, lDetails.data(),
                                static_cast<int>(lDetails.size())))
                        {
                            continue;
                        }

                        LeaderboardEntry lEntry;
                        lEntry.Player.Id = OnlinePlayerId{ EOnlineBackend::Steam, SteamRules::IdText(lSteamEntry.m_steamIDUser.ConvertToUint64()) };
                        lEntry.Rank      = lSteamEntry.m_nGlobalRank;
                        lEntry.Score     = lSteamEntry.m_nScore;

                        const Int32 lDetailCount = Maths::Clamp<Int32>(lSteamEntry.m_cDetails, 0, static_cast<Int32>(SteamRules::MAX_DETAILS));
                        lEntry.Details.assign(lDetails.begin(), lDetails.begin() + lDetailCount);

                        if (lSteamEntry.m_hUGC != k_UGCHandleInvalid)
                        {
                            lEntry.File = OnlineFileId{ SteamRules::IdText(lSteamEntry.m_hUGC) };
                        }
                        lEntries.push_back(Move(lEntry));
                    }

                    InDone(EOnlineResult::Ok, lEntries);
                });
            }

            /**
             * The largest file an entry takes, 0 when entries take none: no leaderboards, no remote
             * storage, or the Steam Cloud off for the app or by the player.
             */
            Uint64 GetMaxFileBytes() const
            {
                if (m_Stats == nullptr || m_Storage == nullptr)
                {
                    return 0;
                }

                uint64 lTotal = 0;
                uint64 lAvailable = 0;
                if (!m_Steam->SteamAPI_ISteamRemoteStorage_GetQuota(m_Storage, &lTotal, &lAvailable))
                {
                    return 0;
                }

                return SteamRules::MaxFileBytes(m_Steam->SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp(m_Storage),
                                                m_Steam->SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount(m_Storage),
                                                lTotal);
            }

            /** Shares the cloud file InFile, written, and attaches its handle to the player's entry on InBoard. */
            void ShareAndAttach(const SteamLeaderboard_t InBoard, const OpaaxString& InFile, const Uint64 InSize,
                                const FOnOnlineDone& InDone)
            {
                const SteamAPICall_t lCall = m_Steam->SteamAPI_ISteamRemoteStorage_FileShare(m_Storage, InFile.CStr());

                Await<RemoteStorageFileShareResult_t>(lCall,
                    [this, InBoard, InFile, InSize, InDone](const RemoteStorageFileShareResult_t* InShared)
                    {
                        if (InShared == nullptr || InShared->m_eResult != k_EResultOK || InShared->m_hFile == k_UGCHandleInvalid)
                        {
                            const bool lbRefused = InShared != nullptr && InShared->m_eResult != k_EResultOK;
                            FinishAttach(InFile, InSize, "shared", lbRefused ? InShared->m_eResult : k_EResultFail, InDone);
                            return;
                        }

                        const SteamAPICall_t lAttach = m_Steam->SteamAPI_ISteamUserStats_AttachLeaderboardUGC(
                            m_Stats, InBoard, InShared->m_hFile);

                        Await<LeaderboardUGCSet_t>(lAttach, [this, InFile, InSize, InDone](const LeaderboardUGCSet_t* InSet)
                        {
                            FinishAttach(InFile, InSize, "attached", InSet != nullptr ? InSet->m_eResult : k_EResultFail, InDone);
                        });
                    });
            }

            /** Ends an attach at the step InStep ("written", "shared", "attached") with Steam's result for it. */
            void FinishAttach(const OpaaxString& InFile, const Uint64 InSize, const char* InStep, const Int32 InSteamResult,
                              const FOnOnlineDone& InDone)
            {
                Algo::RemoveIf(m_FilesInFlight, [&InFile](const OpaaxString& InFlying) { return InFlying == InFile; });

                const EOnlineResult lResult = SteamRules::FileResult(InSteamResult);
                if (lResult == EOnlineResult::Ok)
                {
                    OPAAX_LOG(LogOnline, Info, "{} ({} bytes) attached to its leaderboard entry", InFile.CStr(), InSize);
                }
                else
                {
                    OPAAX_LOG(LogOnline, Warn, "{} ({} bytes) not {}: {} (Steam result {})", InFile.CStr(), InSize, InStep,
                              ToString(lResult), InSteamResult);
                }
                InDone(lResult);
            }

            void OnCallCompleted(const SteamAPICallCompleted_t& InCompleted)
            {
                const auto lFound = Algo::FindIf(m_CallWaits, [&InCompleted](const CallWait& InWait)
                {
                    return InWait.Call == InCompleted.m_hAsyncCall;
                });
                if (lFound == m_CallWaits.end())
                {
                    return;
                }

                // Out of the list first: the answer may start another call.
                const TFunction<void()> lFinish = Move(lFound->Finish);
                m_CallWaits.erase(lFound);
                lFinish();
            }

            void Dispatch(const CallbackMsg_t& InMessage)
            {
                switch (InMessage.m_iCallback)
                {
                    case SteamAPICallCompleted_t::k_iCallback:
                    {
                        OnCallCompleted(*reinterpret_cast<const SteamAPICallCompleted_t*>(InMessage.m_pubParam));
                        break;
                    }

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
            ISteamUserStats*         m_Stats   = nullptr;
            ISteamRemoteStorage*     m_Storage = nullptr;
            HSteamPipe               m_Pipe    = 0;

            bool                m_bRunning       = true;
            bool                m_bOverlayActive = false;
            TDynArray<NameWait> m_NameWaits;
            TDynArray<CallWait> m_CallWaits;

            /** The cloud files being written, shared or attached. */
            TDynArray<OpaaxString> m_FilesInFlight;
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

        // Without its stats interface Steam still gives the player and the overlay; the leaderboards answer Unsupported.
        ISteamUserStats* lStats = lSteam->SteamAPI_SteamUserStats_v013();
        if (lStats == nullptr)
        {
            OPAAX_LOG(LogOnline, Warn, "Steam started without its stats interface: no leaderboards");
        }

        // Without its remote storage the leaderboards still work; a file on an entry answers Unsupported.
        ISteamRemoteStorage* lStorage = lSteam->SteamAPI_SteamRemoteStorage_v016();
        if (lStorage == nullptr)
        {
            OPAAX_LOG(LogOnline, Warn, "Steam started without its remote storage interface: no files on leaderboard entries");
        }

        TUniquePtr<SteamBackend> lBackend = MakeUnique<SteamBackend>(Move(lSteam), lUser, lFriends, lUtils, lStats, lStorage);
        const OnlinePlayer lPlayer = lBackend->GetLocalPlayer();
        OPAAX_LOG(LogOnline, Info, "Steam started: app {}, player {} ({}){}", lBackend->GetAppId(), lPlayer.Name.CStr(),
                  lPlayer.Id.Value.CStr(), lBackend->IsLoggedOn() ? "" : ", offline");
        return lBackend;
    }
}
