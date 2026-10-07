// Suite: 2D lighting on the CPU side — which lights reach a view and how they are packed for the
// sprite shader (LightsBlock2D), which of them get shadows and where the casters are drawn, plus
// the material data asset. The shading itself needs a GPU: it is checked by captures and the CI
// render check.
#include <doctest.h>

#include <cmath>
#include <string>

#include <nlohmann/json.hpp>

#include "Renderer/Components/Light2DComponent.h"
#include "Renderer/Components/ShadowCaster2DComponent.h"
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

// =============================================================================
// Shadows: which lights get a shadow map row, and where the casters are drawn.
// =============================================================================

TEST_CASE("Shadows: shadowed point and spot lights get rows, the strongest first; global lights never")
{
    Light2DComponent lWeak   = PointLight(200.f, 1.f);
    Light2DComponent lStrong = PointLight(200.f, 5.f);
    Light2DComponent lPlain  = PointLight(200.f, 9.f);
    Light2DComponent lSun    = PointLight(1.f, 1.f);
    lWeak.bCastShadows   = true;
    lStrong.bCastShadows = true;
    lSun.bCastShadows    = true;
    lSun.Type            = ELight2DType::Global;

    lStrong.ShadowSoftness = 2.f;
    lStrong.ShadowStrength = 0.75f;

    LightsBlock2D lBlock;
    PackLights2D({ { &lWeak, { 0.f, 0.f }, 0.f }, { &lStrong, { 10.f, 0.f }, 0.f },
                   { &lPlain, { 20.f, 0.f }, 0.f }, { &lSun, { 0.f, 0.f }, 0.f } },
                 VIEW, WHITE, 1.f, lBlock);
    REQUIRE(lBlock.GetCount() == 4);

    CHECK(lBlock.Params[1].y == 0.f);    // the strongest shadowed light takes the first row
    CHECK(lBlock.Params[0].y == 1.f);
    CHECK(lBlock.Params[2].y == -1.f);   // casts no shadow
    CHECK(lBlock.Params[3].y == -1.f);   // a global light has no shadow map

    CHECK(lBlock.Params[1].z == doctest::Approx(2.f));
    CHECK(lBlock.Params[1].w == doctest::Approx(0.75f));
}

TEST_CASE("Shadows: past 16 shadowed lights, the weakest go without")
{
    TDynArray<Light2DComponent> lComponents(20);
    TDynArray<Light2DInstance>  lLights;
    for (Uint32 lIndex = 0; lIndex < 20; ++lIndex)
    {
        lComponents[lIndex]              = PointLight(100.f, 1.f + static_cast<float>(lIndex));
        lComponents[lIndex].bCastShadows = true;
        lLights.push_back({ &lComponents[lIndex], { 0.f, 0.f }, 0.f });
    }

    LightsBlock2D lBlock;
    PackLights2D(lLights, VIEW, WHITE, 1.f, lBlock);
    REQUIRE(lBlock.GetCount() == 20);

    // Lights 0..3 are the weakest; the others share rows 0..15, the strongest (19) first.
    TDynArray<bool> lRowUsed(MAX_SHADOWED_LIGHTS_2D, false);
    for (Uint32 lIndex = 0; lIndex < 20; ++lIndex)
    {
        const float lRow = lBlock.Params[lIndex].y;
        if (lIndex < 4)
        {
            CHECK(lRow == -1.f);
            continue;
        }

        REQUIRE(lRow >= 0.f);
        REQUIRE(lRow < static_cast<float>(MAX_SHADOWED_LIGHTS_2D));
        CHECK_FALSE(lRowUsed[static_cast<Uint32>(lRow)]);
        lRowUsed[static_cast<Uint32>(lRow)] = true;
    }
    CHECK(lBlock.Params[19].y == 0.f);
}

TEST_CASE("Shadows: the shadow block lists each row's light; cleared rows list none")
{
    Light2DComponent lWeak   = PointLight(150.f, 1.f);
    Light2DComponent lStrong = PointLight(250.f, 4.f);
    lWeak.bCastShadows   = true;
    lStrong.bCastShadows = true;

    LightsBlock2D lBlock;
    PackLights2D({ { &lWeak, { -10.f, 5.f }, 0.f }, { &lStrong, { 30.f, 40.f }, 0.f } }, VIEW, WHITE, 1.f, lBlock);

    ShadowBlock2D lShadows;
    REQUIRE(BuildShadowBlock2D(lBlock, lShadows) == 2);
    CHECK(lShadows.GetCount() == 2);
    CHECK(lShadows.Info.y == doctest::Approx(static_cast<float>(SHADOW_MAP_ANGLES_2D)));

    CHECK(lShadows.Light[0].x == doctest::Approx(30.f));   // row 0: the strong light
    CHECK(lShadows.Light[0].y == doctest::Approx(40.f));
    CHECK(lShadows.Light[0].z == doctest::Approx(250.f));
    CHECK(lShadows.Light[1].x == doctest::Approx(-10.f));
    CHECK(lShadows.Light[1].z == doctest::Approx(150.f));

    ClearShadowRows2D(lBlock);
    CHECK(lBlock.Params[0].y == -1.f);
    CHECK(lBlock.Params[1].y == -1.f);
    CHECK(BuildShadowBlock2D(lBlock, lShadows) == 0);
}

TEST_CASE("Shadows: without shadowed lights, the occlusion map is the view at screen density")
{
    const OcclusionLayout2D lLayout = MakeOcclusionLayout2D(VIEW, 1.2f, ShadowBlock2D{});

    CHECK(lLayout.Width == 1200);
    CHECK(lLayout.Height == 720);
    CHECK(lLayout.Bounds.Center.x == doctest::Approx(0.f));
    CHECK(lLayout.Bounds.Center.y == doctest::Approx(0.f));
    CHECK(lLayout.Bounds.HalfExtent.x == doctest::Approx(500.f));
    CHECK(lLayout.Bounds.HalfExtent.y == doctest::Approx(300.f));
}

TEST_CASE("Shadows: the occlusion map takes in a shadowed light's reach, up to three times the view")
{
    ShadowBlock2D lShadows;
    lShadows.Info.x   = 2.f;
    lShadows.Light[0] = Vector4F{ 600.f, 0.f, 300.f, 0.f };    // reaches x = 900, beyond the view's 500
    lShadows.Light[1] = Vector4F{ 0.f, 5000.f, 400.f, 0.f };   // far above: cut at 3 x 300 = 900

    const OcclusionLayout2D lLayout = MakeOcclusionLayout2D(VIEW, 1.f, lShadows);

    // x: -500 .. 900, y: -300 .. 900.
    CHECK(lLayout.Width == 1400);
    CHECK(lLayout.Height == 1200);
    CHECK(lLayout.Bounds.Center.x == doctest::Approx(200.f));
    CHECK(lLayout.Bounds.Center.y == doctest::Approx(300.f));
    CHECK(lLayout.Bounds.HalfExtent.x == doctest::Approx(700.f));
    CHECK(lLayout.Bounds.HalfExtent.y == doctest::Approx(600.f));
}

TEST_CASE("Shadows: a dense occlusion map stops at its largest size and keeps whole pixels")
{
    // 10 pixels a unit would be 10000 x 6000: it gets coarser instead.
    const OcclusionLayout2D lLayout = MakeOcclusionLayout2D(VIEW, 10.f, ShadowBlock2D{});

    CHECK(lLayout.Width == OCCLUSION_MAX_SIZE_2D);
    CHECK(lLayout.Height == 2458);   // 6000 * 4096 / 10000, rounded up
    CHECK(lLayout.Bounds.HalfExtent.x == doctest::Approx(500.f));

    // The bounds match the pixels' aspect exactly, so a view of Height pixels over HalfExtent.y
    // spans HalfExtent.x across Width.
    CHECK(lLayout.Bounds.HalfExtent.y / lLayout.Bounds.HalfExtent.x
          == doctest::Approx(static_cast<float>(lLayout.Height) / static_cast<float>(lLayout.Width)));
}

// =============================================================================
// Ambient occlusion: a blurred, smaller copy of the occlusion map.
// =============================================================================

TEST_CASE("Ambient occlusion: its map is a quarter of the occlusion map each way, at least one texel")
{
    CHECK(AmbientOcclusionExtent2D(1200) == 300);
    CHECK(AmbientOcclusionExtent2D(1201) == 301);
    CHECK(AmbientOcclusionExtent2D(3) == 1);
    CHECK(AmbientOcclusionExtent2D(0) == 1);
}

TEST_CASE("Ambient occlusion: the blur is half the radius, in the ambient occlusion map's texels")
{
    // 1.2 occlusion texels a unit, 4 of them to an ambient occlusion texel: 48 units -> 24 -> 7.2.
    const OcclusionLayout2D lLayout = MakeOcclusionLayout2D(VIEW, 1.2f, ShadowBlock2D{});
    CHECK(AmbientOcclusionSigma2D(48.f, lLayout) == doctest::Approx(7.2f));
    CHECK(AmbientOcclusionSigma2D(-5.f, lLayout) == doctest::Approx(0.f));
}

TEST_CASE("Ambient occlusion: packing places its map and strength; packing the lights turns it off")
{
    const OcclusionLayout2D lLayout = MakeOcclusionLayout2D(VIEW, 1.f, ShadowBlock2D{});

    LightsBlock2D lBlock;
    CHECK(lBlock.AOParams.x == 0.f);   // off by default

    PackAmbientOcclusion2D(lBlock, lLayout, 1.5f);
    CHECK(lBlock.AORect.x == doctest::Approx(-500.f));
    CHECK(lBlock.AORect.y == doctest::Approx(-300.f));
    CHECK(lBlock.AORect.z == doctest::Approx(1000.f));
    CHECK(lBlock.AORect.w == doctest::Approx(600.f));
    CHECK(lBlock.AOParams.x == doctest::Approx(1.f));   // at most full strength

    PackLights2D({}, VIEW, WHITE, 1.f, lBlock);
    CHECK(lBlock.AOParams.x == 0.f);
}

TEST_CASE("Ambient occlusion: its map reaches past an occlusion map that is not a multiple of its texels")
{
    // 1001 occlusion texels across: 251 ambient occlusion texels cover 1004 of them.
    OcclusionLayout2D lLayout;
    lLayout.Bounds = Bounds2D{ { 0.f, 0.f }, { 500.5f, 300.f } };
    lLayout.Width  = 1001;
    lLayout.Height = 600;

    LightsBlock2D lBlock;
    PackAmbientOcclusion2D(lBlock, lLayout, 0.5f);

    CHECK(lBlock.AORect.x == doctest::Approx(-500.5f));
    CHECK(lBlock.AORect.z == doctest::Approx(1004.f));
    CHECK(lBlock.AORect.w == doctest::Approx(600.f));
    CHECK(lBlock.AOParams.x == doctest::Approx(0.5f));
}

TEST_CASE("ShadowCaster2D and a light's shadow settings round-trip; older lights load without shadows")
{
    ShadowCaster2DComponent lCaster;
    lCaster.bSelfShadows = true;
    lCaster.bEnabled     = false;

    const ShadowCaster2DComponent lCasterBack = nlohmann::json(lCaster).get<ShadowCaster2DComponent>();
    CHECK(lCasterBack.bSelfShadows);
    CHECK_FALSE(lCasterBack.bEnabled);

    Light2DComponent lLight;
    lLight.bCastShadows   = true;
    lLight.ShadowSoftness = 1.5f;
    lLight.ShadowStrength = 0.25f;

    const Light2DComponent lLightBack = nlohmann::json(lLight).get<Light2DComponent>();
    CHECK(lLightBack.bCastShadows);
    CHECK(lLightBack.ShadowSoftness == doctest::Approx(1.5f));
    CHECK(lLightBack.ShadowStrength == doctest::Approx(0.25f));

    const Light2DComponent lOld = nlohmann::json{ { "Radius", 123.0 } }.get<Light2DComponent>();
    CHECK_FALSE(lOld.bCastShadows);
    CHECK(lOld.Radius == doctest::Approx(123.f));
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
