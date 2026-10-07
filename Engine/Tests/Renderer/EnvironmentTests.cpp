// Suite: EnvironmentComponent — the per-world picture settings saved in maps, and the post-process
// settings a view is composited with. The HDR passes themselves need a GPU: they are checked by
// the render check in CI (Mesa, software OpenGL) and by captures.
#include <doctest.h>

#include <string>

#include <nlohmann/json.hpp>

#include "Renderer/Components/EnvironmentComponent.h"
#include "Renderer/Post/ScenePipeline2D.h"

using namespace Opaax;

TEST_CASE("Environment: a map entry round-trips, and a missing key keeps its default")
{
    EnvironmentComponent lEnvironment;
    lEnvironment.Exposure   = 1.5f;
    lEnvironment.Tonemapper = ETonemapper::Reinhard;

    const nlohmann::json lJson = lEnvironment;
    CHECK(lJson.at("Tonemapper") == "Reinhard");

    const EnvironmentComponent lBack = lJson.get<EnvironmentComponent>();
    CHECK(lBack.Exposure == doctest::Approx(1.5f));
    CHECK(lBack.Tonemapper == ETonemapper::Reinhard);

    const EnvironmentComponent lPartial = nlohmann::json{ { "Exposure", -2.0 } }.get<EnvironmentComponent>();
    CHECK(lPartial.Exposure == doctest::Approx(-2.f));
    CHECK(lPartial.Tonemapper == ETonemapper::ACES);
}

TEST_CASE("Environment: the ambient light and its occlusion round-trip; older environments load without occlusion")
{
    EnvironmentComponent lEnvironment;
    lEnvironment.AmbientIntensity  = 0.25f;
    lEnvironment.bAmbientOcclusion = true;
    lEnvironment.AORadius          = 80.f;
    lEnvironment.AOStrength        = 0.3f;

    const EnvironmentComponent lBack = nlohmann::json(lEnvironment).get<EnvironmentComponent>();
    CHECK(lBack.AmbientIntensity == doctest::Approx(0.25f));
    CHECK(lBack.bAmbientOcclusion);
    CHECK(lBack.AORadius == doctest::Approx(80.f));
    CHECK(lBack.AOStrength == doctest::Approx(0.3f));

    const EnvironmentComponent lOld = nlohmann::json{ { "Exposure", 1.0 } }.get<EnvironmentComponent>();
    CHECK_FALSE(lOld.bAmbientOcclusion);
    CHECK(lOld.AORadius == doctest::Approx(48.f));
    CHECK(lOld.AmbientIntensity == doctest::Approx(1.f));
}

TEST_CASE("Environment: a view's post settings come from it")
{
    EnvironmentComponent lEnvironment;
    lEnvironment.Exposure   = -1.f;
    lEnvironment.Tonemapper = ETonemapper::None;

    const PostSettings lPost = PostSettings::From(lEnvironment);
    CHECK(lPost.ExposureStops == doctest::Approx(-1.f));
    CHECK(lPost.Tonemapper == ETonemapper::None);

    CHECK(std::string(ToString(ETonemapper::ACES)) == "ACES");
}
