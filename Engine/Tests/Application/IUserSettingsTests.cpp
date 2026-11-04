// Suite: the player's settings (IUserSettings), over a project's configs (a ConfigSystem on a temp
// Configs/ folder) and kept in the Settings slot of a save storage. Active, a value is read by the engine
// at once; it lasts once saved, in a file laid out as a save; a value a config does not take refuses the
// change; inactive (the editor outside Play), the configs read the project's values alone.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ISaveStorage.h"
#include "Application/Services/IUserSettings.h"
#include "Automation/SettingsAutomationCommands.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/Config/TConfig.h"

using namespace Opaax;

namespace UserSettingsTestTypes
{
    struct TestGameSettingsData
    {
        float MouseSensitivity = 1.f;
        bool  bShowTimer       = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TestGameSettingsData, MouseSensitivity, bShowTimer)
        OPAAX_PROPERTIES(TestGameSettingsData, OPAAX_PROP(MouseSensitivity), OPAAX_PROP(bShowTimer))
    };

    // A game's own config, registered after the engine's.
    class Config_TestGame final : public TConfig<TestGameSettingsData>
    {
        OPAAX_OBJECT(Config_TestGame, Config)

    public:
        const char* FileName() const override { return "TestGame.config"; }
    };
}

using namespace UserSettingsTestTypes;

namespace
{
    ProjectLayout LayoutIn(const TestTempDir& InTemp)
    {
        ProjectLayout lLayout;
        lLayout.ConfigsDir = InTemp.Sub("Configs");
        return lLayout;
    }

    nlohmann::json Json(const char* InText) { return nlohmann::json::parse(InText); }

    const AudioBusSettings* FindBus(const EngineConfigData& InData, const char* InName)
    {
        for (const AudioBusSettings& lBus : InData.Audio.Buses)
        {
            if (lBus.Name == OPAAX_ID(InName)) { return &lBus; }
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // A project's configs, the player's save folder, and the settings over them: an app's, made again
    // by Relaunch as the next launch makes them.
    // -------------------------------------------------------------------------
    struct Fixture
    {
        TestTempDir Temp{ "user_settings" };
        Paths       ProjectPaths{ LayoutIn(Temp) };
        SaveStorage Storage{ Temp.Sub("SaveGames") };

        TUniquePtr<ConfigSystem> Configs  = MakeUnique<ConfigSystem>(ProjectPaths);
        TUniquePtr<UserSettings> Settings = MakeUnique<UserSettings>(Storage, *Configs);

        Fixture() { Configs->Register<Config_Engine>(); }

        /** What the engine reads. */
        const EngineConfigData& Read() { return Configs->Get<Config_Engine>().GetData(); }

        /** As a game starts: the engine config, the settings read and put over the configs. */
        void Relaunch()
        {
            Settings.reset();
            Configs = MakeUnique<ConfigSystem>(ProjectPaths);
            Configs->Register<Config_Engine>();
            Settings = MakeUnique<UserSettings>(Storage, *Configs);
            Settings->Load();
            Settings->Activate();
        }

        void WriteFile(const char* InText)
        {
            REQUIRE(Storage.Write(IUserSettings::SLOT, OpaaxString(InText)) == ESaveResult::Ok);
        }

        OpaaxString ReadFile()
        {
            OpaaxString lText;
            REQUIRE(Storage.Read(IUserSettings::SLOT, lText) == ESaveResult::Ok);
            return lText;
        }
    };
}

// =============================================================================
// Over the configs
// =============================================================================

TEST_CASE("UserSettings: without a file, the project's values; a value set is read by the engine at once")
{
    Fixture lFix;
    CHECK(lFix.Settings->Load() == ESaveResult::Missing);
    lFix.Settings->Activate();
    CHECK(lFix.Read().Audio.MasterVolume == doctest::Approx(1.f));

    REQUIRE(lFix.Settings->SetMasterVolume(0.25f));
    CHECK(lFix.Read().Audio.MasterVolume == doctest::Approx(0.25f));

    // The project's values, and their file, are not the player's.
    CHECK(lFix.Configs->Get<Config_Engine>().GetProjectData().Audio.MasterVolume == doctest::Approx(1.f));
    CHECK_FALSE(lFix.Configs->Get<Config_Engine>().ToText().Contains("0.25"));
    CHECK(lFix.Settings->HasUnsavedChanges());
    CHECK_FALSE(lFix.Storage.Exists(IUserSettings::SLOT));
}

TEST_CASE("UserSettings: a value saved is read again at the next launch")
{
    Fixture lFix;
    lFix.Settings->Activate();
    REQUIRE(lFix.Settings->SetWindowMode(EWindowMode::Borderless));
    REQUIRE(lFix.Settings->SetBusVolume(OPAAX_ID("Music"), 0.5f));
    REQUIRE(lFix.Settings->Save() == ESaveResult::Ok);
    CHECK_FALSE(lFix.Settings->HasUnsavedChanges());

    lFix.Relaunch();
    CHECK(lFix.Read().Window.Mode == EWindowMode::Borderless);
    CHECK(FindBus(lFix.Read(), "Music")->Volume == doctest::Approx(0.5f));
    CHECK(FindBus(lFix.Read(), "SFX")->Volume == doctest::Approx(1.f));
    CHECK_FALSE(lFix.Settings->HasUnsavedChanges());

    // Not saved: gone at the next launch.
    REQUIRE(lFix.Settings->SetVSync(false));
    lFix.Relaunch();
    CHECK(lFix.Read().Window.bVSync);
}

TEST_CASE("UserSettings: inactive, the configs read the project's values; the values wait in memory")
{
    Fixture lFix;
    lFix.WriteFile(R"({ "Class": "UserSettings", "Properties": { "Configs": { "Engine": { "Window": { "Width": 1000 } } } } })");

    // The editor outside Play: read, not over the configs.
    REQUIRE(lFix.Settings->Load() == ESaveResult::Ok);
    CHECK_FALSE(lFix.Settings->IsActive());
    CHECK(lFix.Read().Window.Width == 1280u);

    lFix.Settings->Activate();
    CHECK(lFix.Read().Window.Width == 1000u);

    lFix.Settings->Deactivate();
    CHECK(lFix.Read().Window.Width == 1280u);
    CHECK_FALSE(lFix.Configs->Get<Config_Engine>().HasPlayerValues());
    CHECK((Json(lFix.Settings->GetConfigValues("Engine").CStr()) == Json(R"({"Window": {"Width": 1000}})")));

    // Set while inactive: checked and kept, read once active.
    REQUIRE(lFix.Settings->SetResolution(1600, 900));
    CHECK(lFix.Read().Window.Width == 1280u);
    lFix.Settings->Activate();
    CHECK(lFix.Read().Window.Width == 1600u);
    CHECK(lFix.Read().Window.Height == 900u);
}

TEST_CASE("UserSettings: a config registered later gets the player's values for it")
{
    Fixture lFix;
    lFix.WriteFile(R"({ "Class": "UserSettings", "Properties": { "Configs": { "TestGame": { "MouseSensitivity": 2.5 } } } })");
    REQUIRE(lFix.Settings->Load() == ESaveResult::Ok);
    lFix.Settings->Activate();

    // A game module's config, registered when the engine starts.
    const Config_TestGame& lGame = lFix.Configs->Register<Config_TestGame>();
    CHECK(lGame.GetData().MouseSensitivity == doctest::Approx(2.5f));
    CHECK(lGame.GetData().bShowTimer);

    REQUIRE(lFix.Settings->SetConfigValues("TestGame", OpaaxString(R"({"bShowTimer": false})")));
    CHECK_FALSE(lGame.GetData().bShowTimer);
}

// =============================================================================
// What a config does not take
// =============================================================================

TEST_CASE("UserSettings: a value the config does not take refuses the change whole, and says why")
{
    Fixture lFix;
    lFix.Settings->Activate();
    REQUIRE(lFix.Settings->SetVSync(false));

    TDynArray<OpaaxString> lWhy;
    CHECK_FALSE(lFix.Settings->SetConfigValues("Engine", OpaaxString(R"({"Window": {"Mod": "Fullscreen", "Width": 1920}})"),
                                               &lWhy));
    REQUIRE(lWhy.size() == 1u);
    CHECK(lWhy[0] == "Window.Mod: no such value");
    CHECK(lFix.Read().Window.Width == 1280u);   // the good one is refused with it
    CHECK_FALSE(lFix.Read().Window.bVSync);     // and what was set stays

    lWhy.clear();
    CHECK_FALSE(lFix.Settings->SetConfigValues("Engine", OpaaxString(R"({"Window": {"Width": "wide"}})"), &lWhy));
    CHECK(lWhy[0] == "Window.Width: a value of the wrong type");

    lWhy.clear();
    CHECK_FALSE(lFix.Settings->SetConfigValues("Engin", OpaaxString(R"({"Window": {"Width": 1}})"), &lWhy));
    CHECK(lWhy[0] == "no config 'Engin'");

    CHECK_FALSE(lFix.Settings->SetConfigValues("Engine", OpaaxString("[1]")));
    CHECK_FALSE(lFix.Settings->SetBusVolume(OPAAX_ID("Voice"), 0.5f));
    CHECK_FALSE(lFix.Settings->SetResolution(0, 720));

    CHECK((Json(lFix.Settings->GetConfigValues("Engine").CStr()) == Json(R"({"Window": {"bVSync": false}})")));
}

TEST_CASE("UserSettings: a file's value a config does not take is dropped, with a warning; the others are read")
{
    Fixture lFix;
    lFix.WriteFile(R"({ "Class": "UserSettings", "Properties": { "Configs": {
                         "Engine": { "Window": { "Mod": "Fullscreen", "Width": 1000 } },
                         "NotAConfig": { "Value": 1 } } } })");
    REQUIRE(lFix.Settings->Load() == ESaveResult::Ok);
    lFix.Settings->Activate();

    CHECK(lFix.Read().Window.Width == 1000u);
    CHECK((Json(lFix.Settings->GetConfigValues("Engine").CStr()) == Json(R"({"Window": {"Width": 1000}})")));

    // No config of that name in this app: kept for one that may come, as it was.
    CHECK((Json(lFix.Settings->GetConfigValues("NotAConfig").CStr()) == Json(R"({"Value": 1})")));

    // The next save writes what is read.
    CHECK(lFix.Settings->HasUnsavedChanges());
}

// =============================================================================
// Taking values away
// =============================================================================

TEST_CASE("UserSettings: a null, ResetConfig and Reset take values away: the project's are read again")
{
    Fixture lFix;
    lFix.Settings->Activate();
    REQUIRE(lFix.Settings->SetConfigValues("Engine", OpaaxString(R"({"Window": {"Mode": "Fullscreen", "bVSync": false},
                                                                      "Audio": {"MasterVolume": 0.5}})")));

    REQUIRE(lFix.Settings->SetConfigValues("Engine", OpaaxString(R"({"Window": {"Mode": null}})")));
    CHECK(lFix.Read().Window.Mode == EWindowMode::Windowed);
    CHECK_FALSE(lFix.Read().Window.bVSync);

    REQUIRE(lFix.Settings->SetSection("Keys", Json(R"([{"Action": "Jump", "Key": "Space"}])")));
    lFix.Settings->ResetConfig("Engine");
    CHECK(lFix.Read().Window.bVSync);
    CHECK(lFix.Read().Audio.MasterVolume == doctest::Approx(1.f));
    CHECK(lFix.Settings->GetConfigValues("Engine") == "{}");
    CHECK_FALSE(lFix.Settings->GetSection("Keys").is_null());

    REQUIRE(lFix.Settings->SetMasterVolume(0.5f));
    lFix.Settings->Reset();
    CHECK(lFix.Read().Audio.MasterVolume == doctest::Approx(1.f));
    CHECK(lFix.Settings->GetSection("Keys").is_null());
    CHECK((lFix.Settings->GetProperties() == Json("{}")));
}

// =============================================================================
// The file
// =============================================================================

TEST_CASE("UserSettings: the file is laid out as a save, which a person can read")
{
    Fixture lFix;
    lFix.Settings->Activate();
    REQUIRE(lFix.Settings->SetBusVolume(OPAAX_ID("Music"), 0.5f));
    REQUIRE(lFix.Settings->SetBusVolume(OPAAX_ID("Master"), 2.f));   // kept between 0 and 1
    REQUIRE(lFix.Settings->SetSection("Keys", Json(R"([{"Action": "Jump", "Key": "Space"}])")));
    REQUIRE(lFix.Settings->Save() == ESaveResult::Ok);

    const OpaaxString lText = lFix.ReadFile();
    CHECK(lText.Contains("\n    "));   // indented

    CHECK((Json(lText.CStr()) == Json(R"({ "Class": "UserSettings", "ClassVersion": 1, "Version": 1, "Properties": {
              "Configs": { "Engine": { "Audio": { "MasterVolume": 1.0, "Buses": { "Music": { "Volume": 0.5 } } } } },
              "Keys": [ { "Action": "Jump", "Key": "Space" } ] } })")));
    CHECK(lFix.Settings->GetFilePath() == lFix.Storage.GetSlotPath(IUserSettings::SLOT));
}

TEST_CASE("UserSettings: a file of a newer game, or of another class, is not read and never written over")
{
    Fixture lFix;
    const char* const lNewer = R"({ "Class": "UserSettings", "ClassVersion": 2, "Properties": { "Configs": { "Engine": { "Window": { "Width": 1000 } } } } })";
    lFix.WriteFile(lNewer);

    CHECK(lFix.Settings->Load() == ESaveResult::NewerVersion);
    lFix.Settings->Activate();
    CHECK(lFix.Read().Window.Width == 1280u);

    REQUIRE(lFix.Settings->SetVSync(false));
    CHECK(lFix.Settings->Save() == ESaveResult::NewerVersion);
    CHECK(lFix.ReadFile() == lNewer);

    lFix.WriteFile(R"({ "Class": "BhopSave", "Properties": {} })");
    CHECK(lFix.Settings->Load() == ESaveResult::WrongClass);
    CHECK(lFix.Settings->Save() == ESaveResult::WrongClass);

    // A file that is not one at all is the player's settings no longer: the next save replaces it.
    lFix.WriteFile("{ cut sh");
    CHECK(lFix.Settings->Load() == ESaveResult::Corrupt);
    REQUIRE(lFix.Settings->SetVSync(false));
    CHECK(lFix.Settings->Save() == ESaveResult::Ok);
    lFix.Relaunch();
    CHECK_FALSE(lFix.Read().Window.bVSync);
}

TEST_CASE("UserSettings: a system's section is kept as given across launches; Configs is not a section")
{
    Fixture lFix;
    REQUIRE(lFix.Settings->SetSection("Keys", Json(R"({"Jump": "Space"})")));
    CHECK_FALSE(lFix.Settings->SetSection(IUserSettings::CONFIGS_SECTION, Json("{}")));
    CHECK_FALSE(lFix.Settings->SetSection("", Json("{}")));
    REQUIRE(lFix.Settings->Save() == ESaveResult::Ok);

    lFix.Relaunch();
    CHECK((lFix.Settings->GetSection("Keys") == Json(R"({"Jump": "Space"})")));

    REQUIRE(lFix.Settings->SetSection("Keys", nullptr));
    CHECK(lFix.Settings->GetSection("Keys").is_null());
    CHECK(lFix.Settings->HasUnsavedChanges());
}

// =============================================================================
// The slot, and the null settings
// =============================================================================

TEST_CASE("IUserSettings: the settings' slot in any case; the null settings refuse every change")
{
    CHECK(IUserSettings::IsSettingsSlot("Settings"));
    CHECK(IUserSettings::IsSettingsSlot("settings"));
    CHECK(IUserSettings::IsSettingsSlot("SETTINGS"));
    CHECK_FALSE(IUserSettings::IsSettingsSlot("Settings2"));
    CHECK_FALSE(IUserSettings::IsSettingsSlot("Main"));

    IUserSettings& lNull = IUserSettings::Null();
    CHECK(lNull.IsNull());
    CHECK_FALSE(lNull.SetMasterVolume(0.5f));
    CHECK(lNull.GetConfigValues("Engine") == "{}");
    CHECK(lNull.Save() != ESaveResult::Ok);
    CHECK_FALSE(lNull.IsActive());
}

// =============================================================================
// settings.* — the automation commands
// =============================================================================

TEST_CASE("SettingsAutomation: settings.set sets a config's values, settings.save writes them, settings.get shows them")
{
    Fixture lFix;
    lFix.Settings->Activate();

    const AutomationResult lSet = SettingsAutomation::Set(*lFix.Settings, Json(R"({"config": "Engine",
                                                                                   "values": {"Audio": {"MasterVolume": 0.25}}})"));
    REQUIRE(lSet.bOk);
    CHECK(lSet.Value.at("active") == true);
    CHECK(lSet.Value.at("unsaved") == true);
    CHECK((lSet.Value.at("properties") == Json(R"({"Configs": {"Engine": {"Audio": {"MasterVolume": 0.25}}}})")));
    CHECK(lFix.Read().Audio.MasterVolume == doctest::Approx(0.25f));

    const AutomationResult lSave = SettingsAutomation::Save(*lFix.Settings);
    REQUIRE(lSave.bOk);
    CHECK(lSave.Value.at("unsaved") == false);
    CHECK(lSave.Value.at("path").get<OpaaxString>() == lFix.Storage.GetSlotPath(IUserSettings::SLOT));

    // Refused whole, with the config's reason.
    const AutomationResult lTypo = SettingsAutomation::Set(*lFix.Settings, Json(R"({"config": "Engine",
                                                                                    "values": {"Audio": {"MasterVolum": 0.5}}})"));
    CHECK_FALSE(lTypo.bOk);
    CHECK(lTypo.Error.Contains("Audio.MasterVolum: no such value"));
    CHECK_FALSE(SettingsAutomation::Set(*lFix.Settings, Json(R"({"values": {}})")).bOk);
    CHECK_FALSE(SettingsAutomation::Set(*lFix.Settings, Json(R"({"config": "Engine"})")).bOk);

    // Taken away, then read back from the file.
    REQUIRE(SettingsAutomation::Reset(*lFix.Settings, Json("{}")).bOk);
    CHECK(lFix.Read().Audio.MasterVolume == doctest::Approx(1.f));
    const AutomationResult lLoad = SettingsAutomation::Load(*lFix.Settings);
    REQUIRE(lLoad.bOk);
    CHECK(lFix.Read().Audio.MasterVolume == doctest::Approx(0.25f));

    CHECK_FALSE(SettingsAutomation::Get(IUserSettings::Null()).bOk);
}
