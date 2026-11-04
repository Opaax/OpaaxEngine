// Suite: the player's values over a config's (Engine/Config/ConfigLayer.h, and TConfig's player
// values). A layer goes key by key, and an item of a named list by its name; the engine reads the
// project's values with the player's over them, while the file, ToText and the Config panel keep to
// the project's. A player's value that does not fit is left out and named.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Engine/Config/ConfigLayer.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Config/TConfig.h"

using namespace Opaax;

namespace
{
    class PlayerProbeConfig final : public TConfig<EngineConfigData>
    {
        OPAAX_OBJECT(PlayerProbeConfig, Config)

    public:
        const char* FileName() const override { return "PlayerProbe.config"; }
    };

    struct ChangeCounter
    {
        Uint32 Calls = 0;
        void   OnChanged() { ++Calls; }
    };

    /** Sets the Uint32 fields named InName, as the Config panel does when one is edited. */
    class SetUint final : public IPropertyVisitor
    {
    public:
        SetUint(const char* InName, const Uint32 InValue) : m_Name(InName), m_Value(InValue) {}

        void Visit(const char* InName, Uint32& InValue, const PropertyMeta&) override
        {
            if (m_Name == InName) { InValue = m_Value; }
        }

        void Visit(const char*, bool&, const PropertyMeta&) override          {}
        void Visit(const char*, Int16&, const PropertyMeta&) override         {}
        void Visit(const char*, Int32&, const PropertyMeta&) override         {}
        void Visit(const char*, float&, const PropertyMeta&) override         {}
        void Visit(const char*, Vector2F&, const PropertyMeta&) override      {}
        void Visit(const char*, Vector3F&, const PropertyMeta&) override      {}
        void Visit(const char*, Vector4F&, const PropertyMeta&) override      {}
        void Visit(const char*, LinearColor&, const PropertyMeta&) override   {}
        void Visit(const char*, OpaaxString&, const PropertyMeta&) override   {}
        void Visit(const char*, OpaaxStringID&, const PropertyMeta&) override {}
        void VisitEnum(const char*, const char* const*, Uint32, Uint32&, const PropertyMeta&) override {}
        void VisitAssetRef(const char*, OpaaxString&, const ObjectClass&, const PropertyMeta&) override {}
        void VisitObjectRef(const char*, ObjectHandle&, const ObjectClass&, const PropertyMeta&) override {}
        void VisitUnsupported(const char*, OpaaxStringView) override {}
        bool BeginGroup(const char*, const PropertyMeta&) override { return true; }
        void EndGroup() override {}

    private:
        OpaaxString m_Name;
        Uint32      m_Value;
    };

    nlohmann::json Json(const char* InText) { return nlohmann::json::parse(InText); }

    const AudioBusSettings* FindBus(const EngineConfigData& InData, const char* InName)
    {
        for (const AudioBusSettings& lBus : InData.Audio.Buses)
        {
            if (lBus.Name == OPAAX_ID(InName)) { return &lBus; }
        }
        return nullptr;
    }
}

// =============================================================================
// ConfigLayer — pure JSON
// =============================================================================
TEST_CASE("ConfigLayer: Leaves splits a layer into its values, each with its path and a layer of its own")
{
    const TDynArray<ConfigLayer::Leaf> lLeaves = ConfigLayer::Leaves(
        Json(R"({"Window": {"Mode": "Fullscreen", "Width": 1920}, "Audio": {"Buses": {"Music": {"Volume": 0.5}}},
                 "Empty": {}})"));

    // In key order; an empty object holds no value.
    REQUIRE(lLeaves.size() == 3u);
    CHECK(lLeaves[0].Path == "Audio.Buses.Music.Volume");
    CHECK((lLeaves[0].Layer == Json(R"({"Audio": {"Buses": {"Music": {"Volume": 0.5}}}})")));
    CHECK(lLeaves[1].Path == "Window.Mode");
    CHECK(lLeaves[2].Path == "Window.Width");
    CHECK((lLeaves[2].Layer == Json(R"({"Window": {"Width": 1920}})")));

    CHECK(ConfigLayer::Leaves(Json("[1, 2]")).empty());
}

TEST_CASE("ConfigLayer: Apply sets objects key by key, and an item of a named list by its name")
{
    nlohmann::json lValues = Json(R"({"Window": {"Mode": "Windowed", "Width": 1280},
                                      "Buses": [{"Name": "Music", "Volume": 1.0}, {"Name": "SFX", "Volume": 1.0}]})");

    REQUIRE(ConfigLayer::Apply(lValues, Json(R"({"Window": {"Width": 1920}, "Buses": {"SFX": {"Volume": 0.25}}})")));
    CHECK((lValues["Window"]["Width"] == 1920));
    CHECK((lValues["Window"]["Mode"] == "Windowed"));
    CHECK((lValues["Buses"][0]["Volume"] == 1.0));
    CHECK((lValues["Buses"][1]["Volume"] == 0.25));
    CHECK(lValues["Buses"].size() == 2u);   // the list stays the values' own

    // No place for it: a key, or a name, the values do not have.
    nlohmann::json lCopy = lValues;
    CHECK_FALSE(ConfigLayer::Apply(lCopy, Json(R"({"Window": {"Hieght": 10}})")));
    CHECK_FALSE(ConfigLayer::Apply(lCopy, Json(R"({"Buses": {"Voice": {"Volume": 0.5}}})")));

    CHECK(ConfigLayer::IsNamedList(lValues["Buses"]));
    CHECK_FALSE(ConfigLayer::IsNamedList(Json("[1, 2]")));
    CHECK_FALSE(ConfigLayer::IsNamedList(Json("[]")));
}

// =============================================================================
// TConfig — the player's values over the project's
// =============================================================================
TEST_CASE("TConfig: the engine reads the player's values over the project's; the file and ToText keep the project's")
{
    const TestTempDir lDir("ConfigPlayer");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath, OpaaxString(R"({"Window":{"Width":900}})"));

    PlayerProbeConfig lConfig;
    REQUIRE(lConfig.Load(lPath));
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Window": {"Mode": "Fullscreen", "Width": 1920}})")));

    CHECK(lConfig.GetData().Window.Mode == EWindowMode::Fullscreen);
    CHECK(lConfig.GetData().Window.Width == 1920u);
    CHECK(lConfig.GetData().Window.Height == 720u);   // the player gave none: the project's
    CHECK(lConfig.GetProjectData().Window.Width == 900u);
    CHECK(lConfig.HasPlayerValues());
    CHECK_FALSE(lConfig.ToText().Contains("1920"));
    CHECK(lConfig.ToEffectiveText().Contains("1920"));

    REQUIRE(lConfig.Save());
    const OpaaxString lFile = FileIO::ReadAllText(lPath);
    CHECK(lFile.Contains("900"));
    CHECK_FALSE(lFile.Contains("1920"));
    CHECK_FALSE(lFile.Contains("Fullscreen"));
}

TEST_CASE("TConfig: the player sets a bus by its name, and the project's list of buses stays")
{
    PlayerProbeConfig lConfig;
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Audio": {"MasterVolume": 0.8, "Buses": {"Music": {"Volume": 0.25}}}})")));

    const AudioBusSettings* const lMusic = FindBus(lConfig.GetData(), "Music");
    const AudioBusSettings* const lSfx   = FindBus(lConfig.GetData(), "SFX");
    REQUIRE(lMusic != nullptr);
    REQUIRE(lSfx != nullptr);
    CHECK(lMusic->Volume == doctest::Approx(0.25f));
    CHECK(lSfx->Volume == doctest::Approx(1.f));
    CHECK(lConfig.GetData().Audio.Buses.size() == DefaultAudioBuses().size());
    CHECK(lConfig.GetData().Audio.MasterVolume == doctest::Approx(0.8f));
    CHECK(FindBus(lConfig.GetProjectData(), "Music")->Volume == doctest::Approx(1.f));
}

TEST_CASE("TConfig: a player's value the config does not have, or of the wrong type, is left out and named")
{
    PlayerProbeConfig      lConfig;
    TDynArray<OpaaxString> lRejected;
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Window": {"Mod": "Fullscreen", "Width": "wide", "Mode": "Fulscreen",
                                                              "bVSync": false},
                                                  "Audio": {"Buses": {"Voice": {"Volume": 0.5}}}})"),
                                  &lRejected));

    // In key order; the one that fits applies.
    REQUIRE(lRejected.size() == 4u);
    CHECK(lRejected[0] == "Audio.Buses.Voice.Volume: no such value");
    CHECK(lRejected[1] == "Window.Mod: no such value");
    CHECK(lRejected[2] == "Window.Mode: a value of the wrong type");
    CHECK(lRejected[3] == "Window.Width: a value of the wrong type");
    CHECK_FALSE(lConfig.GetData().Window.bVSync);
    CHECK(lConfig.GetData().Window.Width == 1280u);
    CHECK((nlohmann::json::parse(lConfig.GetPlayerText().CStr()) == Json(R"({"Window": {"bVSync": false}})")));

    // Not an object: nothing changes.
    CHECK_FALSE(lConfig.SetPlayerText(OpaaxString("[1]")));
    CHECK_FALSE(lConfig.SetPlayerText(OpaaxString("{ not json")));
    CHECK_FALSE(lConfig.GetData().Window.bVSync);
}

TEST_CASE("TConfig: the player's values are announced when what the engine reads changes, and only then")
{
    PlayerProbeConfig lConfig;
    ChangeCounter     lCounter;
    lConfig.OnChanged().AddMember(&lCounter, &ChangeCounter::OnChanged);

    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Window": {"bVSync": false}})")));
    CHECK(lCounter.Calls == 1);

    // The same again, then with the project's own value: the engine reads the same.
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Window": {"bVSync": false}})")));
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Window": {"bVSync": false, "Width": 1280}})")));
    CHECK(lCounter.Calls == 1);

    // Removed: the project's values again.
    REQUIRE(lConfig.SetPlayerText(OpaaxString("{}")));
    CHECK(lCounter.Calls == 2);
    CHECK(lConfig.GetData().Window.bVSync);
    CHECK_FALSE(lConfig.HasPlayerValues());
    CHECK(lConfig.GetPlayerText() == "{}");

    lConfig.OnChanged().RemoveAll(&lCounter);
}

TEST_CASE("TConfig: an edit of the project's values keeps the player's over them")
{
    PlayerProbeConfig lConfig;
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Window": {"Width": 1920}})")));

    OpaaxString lError;
    REQUIRE(lConfig.MergeText(OpaaxString(R"({"Window": {"Width": 800, "Height": 600}})"), lError));
    CHECK(lConfig.GetData().Window.Width == 1920u);    // the player's wins
    CHECK(lConfig.GetData().Window.Height == 600u);    // the project's comes through
    CHECK(lConfig.GetProjectData().Window.Width == 800u);

    // The Config panel draws and edits the project's values.
    SetUint lEdit("Height", 777u);
    REQUIRE(lConfig.VisitValues(lEdit));
    lConfig.NotifyChanged();
    CHECK(lConfig.GetProjectData().Window.Height == 777u);
    CHECK(lConfig.GetData().Window.Height == 777u);
    CHECK(lConfig.GetData().Window.Width == 1920u);

    // And its reset is the project's too.
    lConfig.ResetToDefaults();
    CHECK(lConfig.GetData().Window.Width == 1920u);
    CHECK(lConfig.GetData().Window.Height == 720u);
}

TEST_CASE("TConfig: a project change that takes away what a player's value is for drops that value")
{
    PlayerProbeConfig lConfig;
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Audio": {"Buses": {"Music": {"Volume": 0.25}}}})")));

    // The project keeps one bus, not the player's.
    OpaaxString lError;
    REQUIRE(lConfig.MergeText(OpaaxString(R"({"Audio": {"Buses": [{"Name": "SFX"}]}})"), lError));

    CHECK_FALSE(lConfig.HasPlayerValues());
    REQUIRE(lConfig.GetData().Audio.Buses.size() == 1u);
    CHECK(lConfig.GetData().Audio.Buses[0].Name == OPAAX_ID("SFX"));
}

TEST_CASE("TConfig: a new load of the project's file keeps the player's values over it")
{
    const TestTempDir lDir("ConfigPlayerReload");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath, OpaaxString(R"({"Window":{"Width":900}})"));

    PlayerProbeConfig lConfig;
    REQUIRE(lConfig.Load(lPath));
    REQUIRE(lConfig.SetPlayerText(OpaaxString(R"({"Window": {"bVSync": false}})")));

    FileIO::WriteAllText(lPath, OpaaxString(R"({"Window":{"Width":1000}})"));
    REQUIRE(lConfig.Load(lPath));
    CHECK(lConfig.GetData().Window.Width == 1000u);
    CHECK_FALSE(lConfig.GetData().Window.bVSync);
}
