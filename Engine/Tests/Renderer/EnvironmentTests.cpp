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

TEST_CASE("Environment: bloom reaches the post settings only when on")
{
    EnvironmentComponent lEnvironment;
    lEnvironment.BloomIntensity = 1.5f;
    lEnvironment.BloomThreshold = 2.f;
    lEnvironment.BloomSoftness  = 3.f;   // past its range

    CHECK(PostSettings::From(lEnvironment).BloomIntensity == doctest::Approx(0.f));   // off by default

    lEnvironment.bBloom = true;
    const PostSettings lPost = PostSettings::From(lEnvironment);
    CHECK(lPost.BloomIntensity == doctest::Approx(1.5f));
    CHECK(lPost.BloomThreshold == doctest::Approx(2.f));
    CHECK(lPost.BloomSoftness == doctest::Approx(1.f));

    const EnvironmentComponent lBack = nlohmann::json(lEnvironment).get<EnvironmentComponent>();
    CHECK(lBack.bBloom);
    CHECK(lBack.BloomThreshold == doctest::Approx(2.f));
    CHECK(lBack.BloomIntensity == doctest::Approx(1.5f));
}

TEST_CASE("Bloom: the chain starts at half the scene and halves while both sides keep 4 pixels")
{
    const TDynArray<BloomLevel2D> lFull = MakeBloomLevels2D(1280, 720);
    REQUIRE(lFull.size() == MAX_BLOOM_LEVELS_2D);
    CHECK(lFull[0].Width == 640);
    CHECK(lFull[0].Height == 360);
    CHECK(lFull[5].Width == 20);
    CHECK(lFull[5].Height == 11);

    // 32x16, 16x8, 8x4; 4x2 would be too thin.
    const TDynArray<BloomLevel2D> lSmall = MakeBloomLevels2D(64, 32);
    REQUIRE(lSmall.size() == 3);
    CHECK(lSmall[2].Width == 8);
    CHECK(lSmall[2].Height == 4);

    // A tiny scene still gets its first level.
    const TDynArray<BloomLevel2D> lTiny = MakeBloomLevels2D(3, 1);
    REQUIRE(lTiny.size() == 1);
    CHECK(lTiny[0].Width == 1);
    CHECK(lTiny[0].Height == 1);
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
