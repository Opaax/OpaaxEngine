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
