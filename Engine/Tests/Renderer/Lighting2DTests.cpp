// Suite: 2D lighting on the CPU side — which lights reach a view and how they are packed for the
// sprite shader (LightsBlock2D), plus the material data asset. The shading itself needs a GPU:
// it is checked by captures and the CI render check.
#include <doctest.h>

#include <cmath>
#include <string>

#include <nlohmann/json.hpp>

#include "Renderer/Components/Light2DComponent.h"
#include "Renderer/Components/SpriteComponent.h"
#include "Renderer/Lighting/Lighting2D.h"
#include "Renderer/Materials/Material2D.h"

using namespace Opaax;

namespace
{
    /** A 1000 x 600 view centred on the origin. */
    const Bounds2D VIEW{ { 0.f, 0.f }, { 500.f, 300.f } };

    const Vector3F WHITE{ 1.f, 1.f, 1.f };

    Light2DComponent PointLight(const float InRadius = 200.f, const float InIntensity = 1.f)
    {
        Light2DComponent lLight;
        lLight.Type      = ELight2DType::Point;
        lLight.Radius    = InRadius;
        lLight.Intensity = InIntensity;
        return lLight;
    }
}

TEST_CASE("Lights: the ambient light is decoded to linear and scaled by its intensity")
{
    LightsBlock2D lBlock;
    PackLights2D({}, VIEW, Vector3F{ 0.5f, 1.f, 0.f }, 2.f, lBlock);

    CHECK(lBlock.Ambient.x == doctest::Approx(std::pow(0.5f, 2.2f) * 2.f));
    CHECK(lBlock.Ambient.y == doctest::Approx(2.f));
    CHECK(lBlock.Ambient.z == doctest::Approx(0.f));
    CHECK(lBlock.GetCount() == 0);
}

TEST_CASE("Lights: only lights that can reach the view are kept")
{
    const Light2DComponent lNear      = PointLight(200.f);
    const Light2DComponent lFar       = PointLight(200.f);
    const Light2DComponent lTouching  = PointLight(200.f);
    Light2DComponent       lOff       = PointLight(200.f);
    Light2DComponent       lDark      = PointLight(200.f, 0.f);
    Light2DComponent       lGlobal    = PointLight(1.f);
    lOff.bEnabled = false;
    lGlobal.Type  = ELight2DType::Global;

    const TDynArray<Light2DInstance> lLights =
    {
        { &lNear,     { 100.f, 50.f },     0.f },
        { &lFar,      { 2000.f, 0.f },     0.f },   // 1500 beyond the view's edge
        { &lTouching, { 650.f, 0.f },      0.f },   // 150 from the edge: its radius reaches in
        { &lOff,      { 0.f, 0.f },        0.f },
        { &lDark,     { 0.f, 0.f },        0.f },
        { &lGlobal,   { 99999.f, 99999.f }, 0.f },  // a global light reaches everywhere
    };

    LightsBlock2D lBlock;
    CHECK(PackLights2D(lLights, VIEW, WHITE, 1.f, lBlock) == 3);
    REQUIRE(lBlock.GetCount() == 3);

    CHECK(lBlock.Position[0].x == doctest::Approx(100.f));
    CHECK(lBlock.Position[1].x == doctest::Approx(650.f));
    CHECK(lBlock.Position[2].w == LIGHT_CODE_GLOBAL);
}

TEST_CASE("Lights: a light is packed the way the shader reads it")
{
    Light2DComponent lSpot;
    lSpot.Type         = ELight2DType::Spot;
    lSpot.Color        = LinearColor{ 1.f, 0.5f, 0.f, 1.f };
    lSpot.Intensity    = 3.f;
    lSpot.Radius       = 400.f;
    lSpot.Falloff      = 1.5f;
    lSpot.ConeAngle    = 90.f;
    lSpot.ConeSoftness = 0.5f;
    lSpot.Height       = 80.f;

    LightsBlock2D lBlock;
    PackLights2D({ { &lSpot, { 10.f, 20.f }, 90.f } }, VIEW, WHITE, 1.f, lBlock);
    REQUIRE(lBlock.GetCount() == 1);

    // Where it is, how far it reaches, what it is.
    CHECK(lBlock.Position[0].x == doctest::Approx(10.f));
    CHECK(lBlock.Position[0].y == doctest::Approx(20.f));
    CHECK(lBlock.Position[0].z == doctest::Approx(400.f));
    CHECK(lBlock.Position[0].w == LIGHT_CODE_SPOT);

    // Linear colour times intensity; the falloff exponent rides along.
    CHECK(lBlock.Color[0].x == doctest::Approx(3.f));
    CHECK(lBlock.Color[0].y == doctest::Approx(std::pow(0.5f, 2.2f) * 3.f));
    CHECK(lBlock.Color[0].z == doctest::Approx(0.f));
    CHECK(lBlock.Color[0].w == doctest::Approx(1.5f));

    // Rotated 90 degrees: it points up. A 90 degree cone is 45 degrees each side, the inner half
    // of it (softness 0.5) 22.5.
    CHECK(lBlock.Direction[0].x == doctest::Approx(0.f).epsilon(1e-5));
    CHECK(lBlock.Direction[0].y == doctest::Approx(1.f));
    CHECK(lBlock.Direction[0].z == doctest::Approx(std::cos(3.14159265f / 4.f)));
    CHECK(lBlock.Direction[0].w == doctest::Approx(std::cos(3.14159265f / 8.f)));

    CHECK(lBlock.Params[0].x == doctest::Approx(80.f));
    CHECK(lBlock.Params[0].y == doctest::Approx(-1.f));   // no shadow
}

TEST_CASE("Lights: a global light carries the sine of its elevation")
{
    Light2DComponent lSun;
    lSun.Type      = ELight2DType::Global;
    lSun.Elevation = 30.f;

    LightsBlock2D lBlock;
    PackLights2D({ { &lSun, { 0.f, 0.f }, 0.f } }, VIEW, WHITE, 1.f, lBlock);
    REQUIRE(lBlock.GetCount() == 1);
    CHECK(lBlock.Params[0].x == doctest::Approx(0.5f));
}

TEST_CASE("Lights: past the limit, global lights and the strongest point lights are kept")
{
    // Forty point lights at the centre, of growing intensity, then a weak global light.
    TDynArray<Light2DComponent> lComponents(41);
    TDynArray<Light2DInstance>  lLights;
    for (Uint32 lIndex = 0; lIndex < 40; ++lIndex)
    {
        lComponents[lIndex] = PointLight(100.f, 1.f + static_cast<float>(lIndex));
        lLights.push_back({ &lComponents[lIndex], { 0.f, 0.f }, 0.f });
    }
    lComponents[40]           = PointLight(1.f, 0.1f);
    lComponents[40].Type      = ELight2DType::Global;
    lLights.push_back({ &lComponents[40], { 0.f, 0.f }, 0.f });

    LightsBlock2D lBlock;
    CHECK(PackLights2D(lLights, VIEW, WHITE, 1.f, lBlock) == 41);
    REQUIRE(lBlock.GetCount() == MAX_LIGHTS_2D);

    // The global light first, then the point lights from the strongest down; the 9 weakest are out.
    CHECK(lBlock.Position[0].w == LIGHT_CODE_GLOBAL);
    CHECK(lBlock.Color[1].x == doctest::Approx(40.f));
    float lWeakest = 1.0e9f;
    for (Uint32 lIndex = 1; lIndex < MAX_LIGHTS_2D; ++lIndex) { lWeakest = std::min(lWeakest, lBlock.Color[lIndex].x); }
    CHECK(lWeakest == doctest::Approx(10.f));
}

TEST_CASE("Material2D: a data asset round-trips; a sprite saved before materials still loads")
{
    Material2D lMaterial;
    lMaterial.bLit             = false;
    lMaterial.NormalMap.Path   = OpaaxString("Textures/Hero_N.png");
    lMaterial.EmissiveColor    = LinearColor{ 1.f, 0.2f, 0.f, 1.f };
    lMaterial.EmissiveStrength = 4.f;

    const Material2D lBack = nlohmann::json(lMaterial).get<Material2D>();
    CHECK_FALSE(lBack.bLit);
    CHECK(lBack.NormalMap.Path == "Textures/Hero_N.png");
    CHECK(lBack.EmissiveColor.g == doctest::Approx(0.2f));
    CHECK(lBack.EmissiveStrength == doctest::Approx(4.f));

    const SpriteComponent lOld = nlohmann::json{ { "Size", { { "x", 10.0 }, { "y", 20.0 } } } }.get<SpriteComponent>();
    CHECK(lOld.Material.IsEmpty());
    CHECK(lOld.Size.x == doctest::Approx(10.f));

    SpriteComponent lSprite;
    lSprite.Material.Path = OpaaxString("Materials/Glow.opaaxdata");
    CHECK(nlohmann::json(lSprite).get<SpriteComponent>().Material.Path == "Materials/Glow.opaaxdata");
}
