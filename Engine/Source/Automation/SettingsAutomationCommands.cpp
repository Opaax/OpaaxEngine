#include "Automation/SettingsAutomationCommands.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IUserSettings.h"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax::SettingsAutomation
{
    namespace
    {
        /** InReasons as "A; B; C". */
        OpaaxString JoinReasons(const TDynArray<OpaaxString>& InReasons)
        {
            OpaaxString lJoined;
            for (const OpaaxString& lReason : InReasons)
            {
                if (!lJoined.IsEmpty()) { lJoined += "; "; }
                lJoined += lReason;
            }
            return lJoined;
        }

        IUserSettings& Settings()
        {
            return OpaaxApplication::GetAppService<IUserSettings>();
        }
    }

    void Register(ICommandRegistry& InCommands)
    {
        InCommands.Add("settings.get",
            "The player's settings: {active} over the configs, {unsaved}, {path} their file, {properties} their "
            "values as the file holds them ({\"Configs\": {\"Engine\": ...}}). config.get shows what the engine reads.",
            [](const nlohmann::json&) { return Get(Settings()); });

        InCommands.Add(CommandSpec("settings.set",
            "Sets {values}, shaped like {config}'s file ({\"Audio\": {\"MasterVolume\": 0.5}}; a bus by its name: "
            "{\"Audio\": {\"Buses\": {\"Music\": {\"Volume\": 0.5}}}}), as the player's, as a settings menu does; a "
            "null takes one away. Read at once while a game runs; not saved. Answers as settings.get.")
                .Param("config", ECommandParamType::String, "the config file's name")
                .Param("values", ECommandParamType::Any, "a JSON object shaped like the file"),
            [](const nlohmann::json& InParams) { return Set(Settings(), InParams); });

        InCommands.Add("settings.save", "Writes the player's settings to their file. Answers as settings.get.",
            [](const nlohmann::json&) { return Save(Settings()); });

        InCommands.Add("settings.load",
            "Reads the player's settings from their file again, as a game does when it starts. Answers as settings.get.",
            [](const nlohmann::json&) { return Load(Settings()); });

        InCommands.Add(CommandSpec("settings.reset",
            "Takes the player's values for {config} away, or every one without it; the project's are read "
            "again. Not saved. Answers as settings.get.")
                .Param("config", ECommandParamType::String, "only this config").Optional(),
            [](const nlohmann::json& InParams) { return Reset(Settings(), InParams); });
    }

    AutomationResult Get(const IUserSettings& InSettings)
    {
        if (InSettings.IsNull())
        {
            return AutomationResult::Fail("this app keeps no player's settings");
        }

        return AutomationResult::Ok(nlohmann::json{
            { "active",     InSettings.IsActive() },
            { "unsaved",    InSettings.HasUnsavedChanges() },
            { "path",       InSettings.GetFilePath() },
            { "properties", InSettings.GetProperties() } });
    }

    AutomationResult Set(IUserSettings& InSettings, const nlohmann::json& InParams)
    {
        const OpaaxString lConfig = InParams.value("config", OpaaxString());
        if (lConfig.IsEmpty())
        {
            return AutomationResult::Fail("give the {config}, a config's name (\"Engine\"; config.get names them)");
        }

        const auto lValues = InParams.find("values");
        if (lValues == InParams.end() || !lValues->is_object())
        {
            return AutomationResult::Fail("give {values} shaped like the config's file, like "
                                          "{\"Audio\": {\"MasterVolume\": 0.5}}");
        }

        TDynArray<OpaaxString> lRejected;
        if (!InSettings.SetConfigValues(lConfig, ToJsonText(*lValues), &lRejected))
        {
            return AutomationResult::Fail(lConfig + ": " + JoinReasons(lRejected) + ". Nothing was set.");
        }

        return Get(InSettings);
    }

    AutomationResult Save(IUserSettings& InSettings)
    {
        const ESaveResult lResult = InSettings.Save();
        if (lResult != ESaveResult::Ok)
        {
            return AutomationResult::Fail(OpaaxString("the player's settings were not saved (") + ToString(lResult)
                                          + "): see the log ('" + InSettings.GetFilePath() + "')");
        }

        return Get(InSettings);
    }

    AutomationResult Load(IUserSettings& InSettings)
    {
        const ESaveResult lResult = InSettings.Load();
        if (lResult != ESaveResult::Ok && lResult != ESaveResult::Missing)
        {
            return AutomationResult::Fail(OpaaxString("the player's settings were not read (") + ToString(lResult)
                                          + "): see the log ('" + InSettings.GetFilePath() + "')");
        }

        return Get(InSettings);
    }

    AutomationResult Reset(IUserSettings& InSettings, const nlohmann::json& InParams)
    {
        const OpaaxString lConfig = InParams.value("config", OpaaxString());
        if (lConfig.IsEmpty())
        {
            InSettings.Reset();
        }
        else
        {
            InSettings.ResetConfig(lConfig);
        }

        return Get(InSettings);
    }
}
