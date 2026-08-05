// Suite: EngineConfigData — the pure, tolerant parse/serialize copied from EngineConfig.
// Independent of the IConfigSystem registry (driven with JSON strings).
#include <doctest.h>

#include "Engine/Config/EngineConfigData.h"

using namespace Opaax;

TEST_CASE("ParseEngineConfig: full schema reads every block")
{
    const EngineConfigData lData = ParseEngineConfig(OpaaxString(R"({
        "window":  {"title":"My","width":1920,"height":1080,"mode":"Borderless"},
        "assets":  {"engineRoot":"E/A","engineManifest":"E/A/m.json"},
        "log":     {"level":"warn"},
        "render":  {"backend":"Vulkan","interpolation":false},
        "physics": {"backend":"Box2D","worldBounds":{"enabled":true,"min":[-5,-6],"max":[7,8],"response":"EventOnly"}}
    })"));

    CHECK(lData.WindowTitle  == "My");
    CHECK(lData.WindowWidth  == 1920u);
    CHECK(lData.WindowHeight == 1080u);
    CHECK(lData.WindowMode   == "Borderless");
    CHECK(lData.EngineAssetsRoot      == "E/A");
    CHECK(lData.EngineManifestRelPath == "E/A/m.json");
    CHECK(lData.LogLevel      == "warn");
    CHECK(lData.RenderBackend == "Vulkan");
    CHECK_FALSE(lData.RenderInterpolation);
    CHECK(lData.PhysicsWorldBoundsEnabled);
    CHECK(lData.PhysicsWorldBoundsMin.x == doctest::Approx(-5.f));
    CHECK(lData.PhysicsWorldBoundsMax.y == doctest::Approx(8.f));
    CHECK(lData.PhysicsWorldBoundsResponse == "EventOnly");
}

TEST_CASE("ParseEngineConfig: missing fields keep their defaults")
{
    const EngineConfigData lData = ParseEngineConfig(OpaaxString(R"({"window":{"width":800}})"));

    CHECK(lData.WindowWidth   == 800u);       // overridden
    CHECK(lData.WindowHeight  == 720u);       // default kept
    CHECK(lData.WindowMode    == "Windowed"); // default kept
    CHECK(lData.RenderBackend == "OpenGL");   // default kept
    CHECK(lData.LogLevel      == "trace");    // default kept
}

TEST_CASE("ParseEngineConfig: malformed JSON yields the defaults (no throw)")
{
    const EngineConfigData lData = ParseEngineConfig(OpaaxString("{ not json"));

    CHECK(lData.WindowWidth   == 1280u);
    CHECK(lData.RenderBackend == "OpenGL");
}

TEST_CASE("SerializeEngineConfig -> ParseEngineConfig round-trips")
{
    EngineConfigData lIn;
    lIn.WindowWidth               = 1600;
    lIn.WindowMode                = OpaaxString("Fullscreen");
    lIn.RenderBackend             = OpaaxString("Vulkan");
    lIn.RenderInterpolation       = false;
    lIn.PhysicsWorldBoundsEnabled = true;
    lIn.PhysicsWorldBoundsMin     = { -3.f, -4.f };

    const EngineConfigData lOut = ParseEngineConfig(SerializeEngineConfig(lIn));

    CHECK(lOut.WindowWidth == 1600u);
    CHECK(lOut.WindowMode  == "Fullscreen");
    CHECK(lOut.RenderBackend == "Vulkan");
    CHECK_FALSE(lOut.RenderInterpolation);
    CHECK(lOut.PhysicsWorldBoundsEnabled);
    CHECK(lOut.PhysicsWorldBoundsMin.x == doctest::Approx(-3.f));
}
