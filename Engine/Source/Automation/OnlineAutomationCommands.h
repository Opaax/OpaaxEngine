#pragma once

#include <nlohmann/json.hpp>

#include "Automation/AutomationRunner.h"

namespace Opaax
{
    class IOnlineServices;
    struct OnlineConfigData;

    // =============================================================================
    // OnlineAutomation — the player's store from outside, for tests and agents: what runs and why, a
    //   leaderboard's entries, uploads, files, achievements; with the fake store, its players and its
    //   overlay. A request to the store answers once the store has (10 s at most). An answer the store
    //   gave is a success carrying it ({"result": "Unsupported"}): a script expects it like any value.
    //   The answers are pure functions of the services, tested without an app.
    // =============================================================================
    namespace OnlineAutomation
    {
        /** Registers the online.* commands, over the app's online services. */
        void Register(AutomationRunner& InRunner);

        /** online.info: {backend, reason, available, capabilities, player, overlay, pending, leaderboards, profile}. */
        AutomationResult Info(IOnlineServices& InOnline);

        /**
         * online.use {backend: "none" | "fake", profile}: stops the store and runs that one, the
         * configured leaderboards defined in a fake store's portal. Answers as online.info.
         */
        AutomationResult Use(IOnlineServices& InOnline, const OnlineConfigData& InConfig, const nlohmann::json& InParams);

        /** online.leaderboard.upload {board, score, details, file (text)}: {result, scoreChanged, rank, previousRank}. */
        AutomationResult Upload(IOnlineServices& InOnline, const nlohmann::json& InParams);

        /**
         * online.leaderboard.entries {board, range (World, Friends, AroundPlayer), first, last}:
         * {result, entries: [{rank, score, name, id, details, file}]}.
         */
        AutomationResult Entries(IOnlineServices& InOnline, const nlohmann::json& InParams);

        /** online.file.get {file}: {result, text, size}. */
        AutomationResult GetFile(IOnlineServices& InOnline, const nlohmann::json& InParams);

        /** online.achievements {unlock | clear: name} or {}: {result, achievements: [{name, unlocked}]}. */
        AutomationResult Achievements(IOnlineServices& InOnline, const nlohmann::json& InParams);

        /** online.overlay {active}: opens or closes the fake store's overlay, as the player's key does. */
        AutomationResult Overlay(IOnlineServices& InOnline, const nlohmann::json& InParams);

        /** online.fake.player {name, friend, nameKnown, board, score}: adds a player to the fake store: {id}. */
        AutomationResult AddFakePlayer(IOnlineServices& InOnline, const nlohmann::json& InParams);
    }
}
