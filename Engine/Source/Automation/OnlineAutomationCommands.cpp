#include "Automation/OnlineAutomationCommands.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IOnlineServices.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Time/Clock.h"
#include "Online/Config/Config_Online.h"
#include "Online/Fake/FakeOnlineBackend.h"
#include "Online/OnlineAPI.h"

namespace Opaax::OnlineAutomation
{
    namespace
    {
        /** How long a request waits for the store, in real time. */
        constexpr double STORE_TIMEOUT_SECONDS = 10.0;

        /** What a store request filled in when it answered. */
        struct StoreAnswer
        {
            bool           bDone = false;
            nlohmann::json Value = nlohmann::json::object();
        };

        /** A result that holds the queue until InAnswer is done, then answers its value. */
        AutomationResult WaitFor(const TSharedPtr<StoreAnswer>& InAnswer)
        {
            const double lDeadline = Clock::NowSeconds() + STORE_TIMEOUT_SECONDS;

            AutomationResult lResult;
            lResult.WaitUntil = [InAnswer, lDeadline]() { return InAnswer->bDone || Clock::NowSeconds() >= lDeadline; };
            lResult.Answer    = [InAnswer]()
            {
                if (!InAnswer->bDone)
                {
                    return AutomationResult::Fail("the store gave no answer in 10 s: see online.info for the store "
                                                  "and its pending requests");
                }
                return AutomationResult::Ok(InAnswer->Value);
            };
            return lResult;
        }

        FakeOnlineBackend* FakeStore(IOnlineServices& InOnline)
        {
            return (InOnline.GetBackend() == EOnlineBackend::Fake) ? static_cast<FakeOnlineBackend*>(&InOnline.GetBackendObject())
                                                                   : nullptr;
        }

        nlohmann::json CapabilitiesJson(const OnlineCapabilities& InCapabilities)
        {
            return nlohmann::json{
                { "identity",              InCapabilities.bIdentity },
                { "leaderboards",          InCapabilities.bLeaderboards },
                { "leaderboardsMadeAtRunTime", InCapabilities.bLeaderboardsMadeAtRunTime },
                { "friendsRange",          InCapabilities.bFriendsRange },
                { "leaderboardFiles",      InCapabilities.bLeaderboardFiles },
                { "maxFileBytes",          InCapabilities.MaxFileBytes },
                { "maxDetails",            InCapabilities.MaxDetails },
                { "achievements",          InCapabilities.bAchievements },
                { "overlay",               InCapabilities.bOverlay } };
        }

        nlohmann::json EntryJson(const LeaderboardEntry& InEntry)
        {
            return nlohmann::json{
                { "rank",    InEntry.Rank },
                { "score",   InEntry.Score },
                { "name",    InEntry.Player.Name },
                { "id",      InEntry.Player.Id.Value },
                { "details", InEntry.Details },
                { "file",    InEntry.File.Value } };
        }

        bool ParseRange(const OpaaxString& InText, ELeaderboardRange& OutRange)
        {
            const OpaaxString lText = InText.ToLower();
            for (const ELeaderboardRange lRange : { ELeaderboardRange::World, ELeaderboardRange::Friends,
                                                    ELeaderboardRange::AroundPlayer })
            {
                if (lText == OpaaxString(ToString(lRange)).ToLower())
                {
                    OutRange = lRange;
                    return true;
                }
            }
            return false;
        }

        TDynArray<Uint8> BytesOf(const OpaaxString& InText)
        {
            const Uint8* lData = reinterpret_cast<const Uint8*>(InText.CStr());
            return TDynArray<Uint8>(lData, lData + InText.GetLength());
        }

        /** {board} as a string, or why not. */
        bool BoardOf(const nlohmann::json& InParams, OpaaxString& OutBoard, OpaaxString& OutError)
        {
            const auto lBoard = InParams.find("board");
            if (lBoard == InParams.end() || !lBoard->is_string() || lBoard->get<OpaaxString>().IsEmpty())
            {
                OutError = "\"board\" names one of the game's leaderboards (online.info lists them)";
                return false;
            }
            OutBoard = lBoard->get<OpaaxString>();
            return true;
        }
    }

    // =========================================================================
    // Registration
    // =========================================================================
    void Register(AutomationRunner& InRunner)
    {
        const auto lOnline = []() -> IOnlineServices& { return OpaaxApplication::GetAppService<IOnlineServices>(); };

        InRunner.Register("online.info",
            "The player's store: {backend, reason, available, capabilities, player, overlay, pending, leaderboards, "
            "profile (fake)}.",
            [lOnline](const nlohmann::json&) { return Info(lOnline()); });

        InRunner.Register("online.use",
            "Runs another store: {backend: none | fake, profile: full | steam | epic | gog}. What was asked is "
            "dropped. Answers as online.info.",
            [lOnline](const nlohmann::json& InParams)
            {
                return Use(lOnline(), OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Online>().GetData(),
                           InParams);
            });

        InRunner.Register("online.leaderboard.upload",
            "Uploads {score} to {board}, keeping the player's best, with {details} and {file} (text) attached to a "
            "kept score: {result, scoreChanged, rank, previousRank}.",
            [lOnline](const nlohmann::json& InParams) { return Upload(lOnline(), InParams); });

        InRunner.Register("online.leaderboard.entries",
            "{board}'s entries in {range} (World: ranks first-last; AroundPlayer: offsets; Friends): {result, "
            "entries: [{rank, score, name, id, details, file}]}.",
            [lOnline](const nlohmann::json& InParams) { return Entries(lOnline(), InParams); });

        InRunner.Register("online.file.get", "A file attached to an entry, {file}: {result, text, size}.",
            [lOnline](const nlohmann::json& InParams) { return GetFile(lOnline(), InParams); });

        InRunner.Register("online.achievements",
            "The player's achievements, after {unlock: name} or {clear: name} if given: {result, achievements}.",
            [lOnline](const nlohmann::json& InParams) { return Achievements(lOnline(), InParams); });

        InRunner.Register("online.overlay", "Opens or closes the fake store's overlay, {active}, as the player's key does.",
            [lOnline](const nlohmann::json& InParams) { return Overlay(lOnline(), InParams); });

        InRunner.Register("online.fake.player",
            "Adds a player to the fake store, {name, friend, nameKnown}, with {score} on {board} if given: {id}.",
            [lOnline](const nlohmann::json& InParams) { return AddFakePlayer(lOnline(), InParams); });
    }

    // =========================================================================
    // Commands
    // =========================================================================
    AutomationResult Info(IOnlineServices& InOnline)
    {
        const OnlinePlayer lPlayer = InOnline.GetLocalPlayer();

        nlohmann::json lBoards = nlohmann::json::array();
        for (const LeaderboardDefinition& lBoard : InOnline.GetLeaderboards())
        {
            lBoards.push_back(lBoard.Name);
        }

        nlohmann::json lInfo{
            { "backend",      ToString(InOnline.GetBackend()) },
            { "reason",       InOnline.GetBackendReason() },
            { "available",    InOnline.IsAvailable() },
            { "capabilities", CapabilitiesJson(InOnline.GetCapabilities()) },
            { "player",       nlohmann::json{ { "id", lPlayer.Id.Value }, { "name", lPlayer.Name } } },
            { "overlay",      InOnline.IsOverlayActive() },
            { "pending",      InOnline.GetPendingCount() },
            { "leaderboards", lBoards } };

        if (const FakeOnlineBackend* lFake = FakeStore(InOnline))
        {
            lInfo["profile"] = ToString(lFake->GetProfile());
        }
        return AutomationResult::Ok(lInfo);
    }

    AutomationResult Use(IOnlineServices& InOnline, const OnlineConfigData& InConfig, const nlohmann::json& InParams)
    {
        const OpaaxString lBackend = InParams.value("backend", OpaaxString("fake")).ToLower();

        OnlineBackendChoice lChoice;
        if (lBackend == "fake")
        {
            const OpaaxString lProfile = InParams.value("profile", OpaaxString("full"));
            if (!ParseFakeStoreProfile(lProfile.View(), lChoice.FakeProfile))
            {
                return AutomationResult::Fail(OpaaxString::Format("no fake profile '{}': full, steam, epic or gog",
                                                                  lProfile.CStr()));
            }
            lChoice.Candidates.push_back(EOnlineBackend::Fake);
            lChoice.Reason = OpaaxString::Format("fake store, {} profile, from online.use", ToString(lChoice.FakeProfile));
        }
        else if (lBackend == "none")
        {
            lChoice.Reason = "from online.use";
        }
        else
        {
            return AutomationResult::Fail("\"backend\" is none or fake: a real store is chosen at launch (--online steam)");
        }

        OpaaxString lReason;
        InOnline.ReplaceBackend(OnlineAPI::CreateFirstBackend(lChoice, InConfig, lReason), Move(lReason));
        return Info(InOnline);
    }

    AutomationResult Upload(IOnlineServices& InOnline, const nlohmann::json& InParams)
    {
        OpaaxString lBoard;
        OpaaxString lError;
        if (!BoardOf(InParams, lBoard, lError))
        {
            return AutomationResult::Fail(lError);
        }

        const auto lScore = InParams.find("score");
        if (lScore == InParams.end() || !lScore->is_number_integer())
        {
            return AutomationResult::Fail("\"score\" is a whole number (a time in milliseconds, points)");
        }

        TDynArray<Int32> lDetails;
        if (const auto lGiven = InParams.find("details"); lGiven != InParams.end())
        {
            if (!lGiven->is_array())
            {
                return AutomationResult::Fail("\"details\" is a list of whole numbers");
            }
            for (const nlohmann::json& lDetail : *lGiven)
            {
                if (!lDetail.is_number_integer())
                {
                    return AutomationResult::Fail("\"details\" is a list of whole numbers");
                }
                lDetails.push_back(lDetail.get<Int32>());
            }
        }

        const TDynArray<Uint8> lFile = BytesOf(InParams.value("file", OpaaxString()));

        TSharedPtr<StoreAnswer> lAnswer = MakeShared<StoreAnswer>();
        InOnline.UploadScore(lBoard.View(), lScore->get<Int32>(), lDetails, lFile,
            [lAnswer](const EOnlineResult InResult, const ScoreUploadResult& InUpload)
            {
                lAnswer->Value = nlohmann::json{
                    { "result",       ToString(InResult) },
                    { "scoreChanged", InUpload.bScoreChanged },
                    { "rank",         InUpload.Rank },
                    { "previousRank", InUpload.PreviousRank } };
                lAnswer->bDone = true;
            });
        return WaitFor(lAnswer);
    }

    AutomationResult Entries(IOnlineServices& InOnline, const nlohmann::json& InParams)
    {
        OpaaxString lBoard;
        OpaaxString lError;
        if (!BoardOf(InParams, lBoard, lError))
        {
            return AutomationResult::Fail(lError);
        }

        ELeaderboardRange lRange = ELeaderboardRange::World;
        const OpaaxString lRangeText = InParams.value("range", OpaaxString("World"));
        if (!ParseRange(lRangeText, lRange))
        {
            return AutomationResult::Fail(OpaaxString::Format("no range '{}': World, Friends or AroundPlayer", lRangeText.CStr()));
        }

        const bool  bAround = lRange == ELeaderboardRange::AroundPlayer;
        const Int32 lFirst  = InParams.value("first", bAround ? -5 : 1);
        const Int32 lLast   = InParams.value("last", bAround ? 5 : 10);

        TSharedPtr<StoreAnswer> lAnswer = MakeShared<StoreAnswer>();
        InOnline.DownloadEntries(lBoard.View(), lRange, lFirst, lLast,
            [lAnswer](const EOnlineResult InResult, const TDynArray<LeaderboardEntry>& InEntries)
            {
                nlohmann::json lEntries = nlohmann::json::array();
                for (const LeaderboardEntry& lEntry : InEntries)
                {
                    lEntries.push_back(EntryJson(lEntry));
                }
                lAnswer->Value = nlohmann::json{ { "result", ToString(InResult) }, { "entries", lEntries } };
                lAnswer->bDone = true;
            });
        return WaitFor(lAnswer);
    }

    AutomationResult GetFile(IOnlineServices& InOnline, const nlohmann::json& InParams)
    {
        const OpaaxString lFile = InParams.value("file", OpaaxString());
        if (lFile.IsEmpty())
        {
            return AutomationResult::Fail("\"file\" is an entry's file (online.leaderboard.entries gives it)");
        }

        TSharedPtr<StoreAnswer> lAnswer = MakeShared<StoreAnswer>();
        InOnline.DownloadFile(OnlineFileId{ lFile }, [lAnswer](const EOnlineResult InResult, const TDynArray<Uint8>& InBytes)
        {
            lAnswer->Value = nlohmann::json{
                { "result", ToString(InResult) },
                { "text",   OpaaxString(reinterpret_cast<const char*>(InBytes.data()), static_cast<Uint32>(InBytes.size())) },
                { "size",   InBytes.size() } };
            lAnswer->bDone = true;
        });
        return WaitFor(lAnswer);
    }

    AutomationResult Achievements(IOnlineServices& InOnline, const nlohmann::json& InParams)
    {
        TSharedPtr<StoreAnswer> lAnswer = MakeShared<StoreAnswer>();

        // The list, once the change (if any) is done: the store answers in order.
        const auto lList = [&InOnline, lAnswer](const EOnlineResult InChanged)
        {
            InOnline.GetAchievements([lAnswer, InChanged](const EOnlineResult InResult, const TDynArray<OnlineAchievement>& InList)
            {
                nlohmann::json lAchievements = nlohmann::json::array();
                for (const OnlineAchievement& lAchievement : InList)
                {
                    lAchievements.push_back(nlohmann::json{ { "name", lAchievement.Name }, { "unlocked", lAchievement.bUnlocked } });
                }
                lAnswer->Value = nlohmann::json{
                    { "result",       ToString(InChanged != EOnlineResult::Ok ? InChanged : InResult) },
                    { "achievements", lAchievements } };
                lAnswer->bDone = true;
            });
        };

        const OpaaxString lUnlock = InParams.value("unlock", OpaaxString());
        const OpaaxString lClear  = InParams.value("clear", OpaaxString());
        if (!lUnlock.IsEmpty())
        {
            InOnline.UnlockAchievement(lUnlock, lList);
        }
        else if (!lClear.IsEmpty())
        {
            InOnline.ClearAchievement(lClear, lList);
        }
        else
        {
            lList(EOnlineResult::Ok);
        }
        return WaitFor(lAnswer);
    }

    AutomationResult Overlay(IOnlineServices& InOnline, const nlohmann::json& InParams)
    {
        FakeOnlineBackend* lFake = FakeStore(InOnline);
        if (lFake == nullptr)
        {
            return AutomationResult::Fail("only the fake store's overlay opens from a script: run with --online fake");
        }

        const auto lActive = InParams.find("active");
        if (lActive == InParams.end() || !lActive->is_boolean())
        {
            return AutomationResult::Fail("\"active\" is true to open the overlay, false to close it");
        }

        lFake->SetOverlayActive(lActive->get<bool>());
        return AutomationResult::Ok(nlohmann::json{ { "overlay", InOnline.IsOverlayActive() } });
    }

    AutomationResult AddFakePlayer(IOnlineServices& InOnline, const nlohmann::json& InParams)
    {
        FakeOnlineBackend* lFake = FakeStore(InOnline);
        if (lFake == nullptr)
        {
            return AutomationResult::Fail("players are added to the fake store only: run with --online fake");
        }

        const OpaaxString lName = InParams.value("name", OpaaxString());
        if (lName.IsEmpty())
        {
            return AutomationResult::Fail("\"name\" is the player's name");
        }

        const OnlinePlayerId lId = lFake->AddPlayer(lName, InParams.value("friend", false), InParams.value("nameKnown", true));

        if (InParams.contains("score"))
        {
            OpaaxString lBoard;
            OpaaxString lError;
            if (!BoardOf(InParams, lBoard, lError) || !InParams["score"].is_number_integer())
            {
                return AutomationResult::Fail("a score goes with \"board\" and is a whole number");
            }
            if (!lFake->SetScore(lBoard, lId, InParams["score"].get<Int32>()))
            {
                return AutomationResult::Fail(OpaaxString::Format("the fake store has no board '{}' yet: upload to it "
                                                                  "first, or list it in the online config", lBoard.CStr()));
            }
        }
        return AutomationResult::Ok(nlohmann::json{ { "id", lId.Value } });
    }
}
