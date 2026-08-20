// Suite: EngineConfigData through the GENERIC codec — the one every config now shares with every
// component (NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT + TConfigCodec's default).
//
// The behaviour these cases pin is the behaviour the deleted hand-written parser had, and the
// reason it could be deleted: missing keys keep defaults, malformed input keeps defaults, and the
// round trip is exact. What moved is WHERE tolerance lives — the codec throws, TConfig::Load
// catches, so it is stated once instead of per field.
#include <doctest.h>

#include <filesystem>

#include "Core/Config/TConfig.hpp"
#include "Core/IO/FileIO.h"
#include "Engine/Config/EngineConfigData.h"

using namespace Opaax;

namespace fs = std::filesystem;

namespace
{
    using Codec = TConfigCodec<EngineConfigData>;

    EngineConfigData Parse(const char* InText) { return Codec::FromText(OpaaxString(InText)); }

    // A config over the same data, so Load's behaviour can be exercised without the registry.
    // Its id is fixed rather than stamped with OPAAX_CONFIG_TYPE: nothing registers it.
    class ProbeConfig final : public TConfig<EngineConfigData>
    {
    public:
        const char*  FileName() const override { return "probe.config"; }
        ConfigTypeID GetConfigTypeID() const noexcept override { return 0; }
    };
}

TEST_CASE("EngineConfigData: full schema reads every group")
{
    const EngineConfigData lData = Parse(R"({
        "Window":  {"Title":"My","Width":1920,"Height":1080,"Mode":"Borderless"},
        "Assets":  {"EngineRoot":"E/A","EngineManifest":"E/A/m.json"},
        "Log":     {"Level":"warn"},
        "Render":  {"Backend":"Vulkan","Interpolation":false},
        "Physics": {"Backend":"Box2D","WorldBounds":{"Enabled":true,
                    "Min":{"x":-5,"y":-6},"Max":{"x":7,"y":8},"Response":"EventOnly"}}
    })");

    CHECK(lData.Window.Title  == "My");
    CHECK(lData.Window.Width  == 1920u);
    CHECK(lData.Window.Height == 1080u);
    // Same json text as when this was a string field — it parses into the enumerator now.
    CHECK(lData.Window.Mode   == EWindowMode::Borderless);
    CHECK(lData.Assets.EngineRoot     == "E/A");
    CHECK(lData.Assets.EngineManifest == "E/A/m.json");
    CHECK(lData.Log.Level        == "warn");
    CHECK(lData.Render.Backend   == EBackend::Vulkan);
    CHECK_FALSE(lData.Render.Interpolation);
    CHECK(lData.Physics.WorldBounds.Enabled);
    CHECK(lData.Physics.WorldBounds.Min.x == doctest::Approx(-5.f));
    CHECK(lData.Physics.WorldBounds.Max.y == doctest::Approx(8.f));
    CHECK(lData.Physics.WorldBounds.Response == "EventOnly");
}

TEST_CASE("EngineConfigData: missing fields keep their defaults, at every depth")
{
    // One key, three levels down, and nothing else — the shape of a config written before the rest
    // of the schema existed. _WITH_DEFAULT is what makes this a read rather than a throw.
    const EngineConfigData lData = Parse(R"({"Window":{"Width":800}})");

    CHECK(lData.Window.Width  == 800u);       // overridden
    CHECK(lData.Window.Height == 720u);       // sibling default
    CHECK(lData.Window.Mode   == EWindowMode::Windowed);
    CHECK(lData.Render.Backend == EBackend::OpenGL);  // absent GROUP defaults whole
    CHECK(lData.Log.Level      == "trace");
    CHECK(lData.Physics.WorldBounds.Response == "EventAndDestroy");
}

TEST_CASE("EngineConfigData: round trip through the generic codec is exact")
{
    EngineConfigData lIn;
    lIn.Window.Width               = 1600;
    lIn.Window.Mode                = EWindowMode::Fullscreen;
    lIn.Render.Backend             = EBackend::Vulkan;
    lIn.Render.Interpolation       = false;
    lIn.Physics.WorldBounds.Enabled = true;
    lIn.Physics.WorldBounds.Min     = Vector2F(-1.f, -2.f);

    const EngineConfigData lOut = Codec::FromText(Codec::ToText(lIn));

    CHECK(lOut.Window.Width == 1600u);
    CHECK(lOut.Window.Mode  == EWindowMode::Fullscreen);
    CHECK(lOut.Render.Backend == EBackend::Vulkan);
    CHECK_FALSE(lOut.Render.Interpolation);
    CHECK(lOut.Physics.WorldBounds.Enabled);
    CHECK(lOut.Physics.WorldBounds.Min.x == doctest::Approx(-1.f));
    CHECK(lOut.Physics.WorldBounds.Min.y == doctest::Approx(-2.f));
}

TEST_CASE("EngineConfigData: the file NESTS because the C++ nests")
{
    const nlohmann::json lJson = nlohmann::json::parse(Codec::ToText(EngineConfigData{}).CStr());

    REQUIRE(lJson.contains("Window"));
    CHECK(lJson["Window"].is_object());
    CHECK(lJson["Window"].contains("Title"));
    CHECK(lJson["Physics"]["WorldBounds"].is_object());

    // A Vector2F writes as the object MathsJson defines, not as the array the hand-written
    // serializer used to emit for these two fields alone.
    CHECK(lJson["Physics"]["WorldBounds"]["Min"].contains("x"));
}

// =============================================================================
// Tolerance — moved OUT of the parser and into TConfig::Load, once
// =============================================================================
TEST_CASE("TConfig::Load: a file it cannot parse keeps the defaults and answers FALSE")
{
    // The CODEC is allowed to throw — that is what lets it be one line instead of a hundred.
    CHECK_THROWS(Parse("{ not json"));
    CHECK_THROWS(Parse(R"({"Window":{"Width":"not a number"}})"));

    // LOAD is where that becomes policy: a config file nobody can read must not take a boot down
    // (**BO4c**'s rule, one level lower), and the false is what ConfigSystem turns into a Warn.
    const fs::path lPath = fs::temp_directory_path() / "OpaaxConfigTolerance.config";
    FileIO::WriteAllText(OpaaxString(lPath.string().c_str()), OpaaxString("{ not json at all"));

    ProbeConfig lProbe;
    REQUIRE_FALSE(lProbe.Load(OpaaxString(lPath.string().c_str())));

    // Defaults intact — a half-applied config would be worse than none.
    CHECK(lProbe.GetData().Window.Width == 1280u);
    CHECK(lProbe.GetData().Render.Backend == EBackend::OpenGL);

    fs::remove(lPath);
}

TEST_CASE("TConfig::Load: a file it CAN parse answers true")
{
    const fs::path lPath = fs::temp_directory_path() / "OpaaxConfigGood.config";
    FileIO::WriteAllText(OpaaxString(lPath.string().c_str()), OpaaxString(R"({"Window":{"Width":900}})"));

    ProbeConfig lProbe;
    CHECK(lProbe.Load(OpaaxString(lPath.string().c_str())));
    CHECK(lProbe.GetData().Window.Width == 900u);

    fs::remove(lPath);
}

TEST_CASE("TConfig::Load: a misspelled ENUMERATOR is refused like any other unreadable value")
{
    // The composed case: an enum field went from string to type, so a hand-edited typo is no longer
    // quietly corrected to Windowed by a FromString nobody watches — it is the same event as a
    // string where a number belongs, and ConfigSystem warns naming the file.
    const fs::path lPath = fs::temp_directory_path() / "OpaaxConfigBadEnum.config";
    FileIO::WriteAllText(OpaaxString(lPath.string().c_str()),
                         OpaaxString(R"({"Window":{"Width":900,"Mode":"Borderles"}})"));

    ProbeConfig lProbe;
    CHECK_FALSE(lProbe.Load(OpaaxString(lPath.string().c_str())));

    // Nothing half-applied: the good Width beside the bad Mode did not land either.
    CHECK(lProbe.GetData().Window.Width == 1280u);
    CHECK(lProbe.GetData().Window.Mode  == EWindowMode::Windowed);

    fs::remove(lPath);
}
