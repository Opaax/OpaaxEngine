#pragma once

#include <nlohmann/json.hpp>

#include "Automation/AutomationRunner.h"

namespace Opaax
{
    class IUserSettings;

    // =============================================================================
    // SettingsAutomation — the commands on the player's settings, for tests and agents: read them, set
    //   a config's values as a settings menu does, save them, read them back and take them away
    //   (settings.get, settings.set, settings.save, settings.load, settings.reset). The answers are pure
    //   functions of the settings, tested without an app.
    // =============================================================================
    namespace SettingsAutomation
    {
        /** Registers settings.get, settings.set, settings.save, settings.load and settings.reset. */
        void Register(AutomationRunner& InRunner);

        /**
         * settings.get: {active} whether they are over the configs, {unsaved} whether they differ from the
         * file, {path} the file, {properties} the values as the file holds them ({"Configs": {"Engine": ...}}).
         */
        AutomationResult Get(const IUserSettings& InSettings);

        /**
         * settings.set {config, values}: merges values, shaped like the config's file, into the player's
         * values for it, as a settings menu sets them (a null takes one away); read by the engine at once
         * when active. A value the config does not have, or of the wrong type, refuses them all. Not
         * saved. Answers as settings.get.
         */
        AutomationResult Set(IUserSettings& InSettings, const nlohmann::json& InParams);

        /** settings.save: writes the file. Answers as settings.get. */
        AutomationResult Save(IUserSettings& InSettings);

        /** settings.load: reads the file again, in place of the values in memory. Answers as settings.get. */
        AutomationResult Load(IUserSettings& InSettings);

        /**
         * settings.reset {config}: takes the player's values for config away, or every value without
         * one (keys included); the project's are read again. Not saved. Answers as settings.get.
         */
        AutomationResult Reset(IUserSettings& InSettings, const nlohmann::json& InParams);
    }
}
