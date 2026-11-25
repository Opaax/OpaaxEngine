// Suite: MaterialEditing — a material's window shows the shader's values under the shader's names,
// hides the slots it does not read, and gives a newly picked shader its defaults.
#include <doctest.h>

#include "Editor/Assets/Editors/MaterialEditing.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    PropertyPresentationView ViewOf(const char* InField)
    {
        return PropertyPresentationView{ OpaaxString(InField), OpaaxString(), PropertyMeta{} };
    }

    const ShaderParamList k_Fog = ShaderParams::Parse(
        "// @param Param0 EdgeCut \"Where the fog is cut\" range(0, 1) default(0.5)\n"
        "// @param Param6 Drift default(0.1)\n"
        "// @param Color0 Ink\n");
}

TEST_SUITE("MaterialEditing")
{
    TEST_CASE("a declared slot shows under its name, with its tooltip and its range")
    {
        PropertyPresentationView lView = ViewOf("Param0");
        REQUIRE(MaterialEditing::Present("Param0", &k_Fog, lView));
        CHECK(lView.Label == "EdgeCut");
        CHECK(lView.Tooltip == "Where the fog is cut\nParam0 in the material file, EdgeCut in the shader");
        CHECK(lView.Meta.RangeMin == doctest::Approx(0.f));
        CHECK(lView.Meta.RangeMax == doctest::Approx(1.f));
        CHECK(lView.Meta.DragStep == doctest::Approx(1.f / MaterialEditing::RANGE_DRAG_STEPS));

        PropertyPresentationView lInk = ViewOf("Color0");
        REQUIRE(MaterialEditing::Present("Color0", &k_Fog, lInk));
        CHECK(lInk.Label == "Ink");
        CHECK(lInk.Tooltip == "Color0 in the material file, Ink in the shader");
        CHECK(lInk.Meta.RangeMin == lInk.Meta.RangeMax);
    }

    TEST_CASE("a slot the shader does not declare is hidden; the other fields stay as they are")
    {
        for (const char* lHidden : { "Param1", "Param7", "Color1", "Texture0", "Texture1" })
        {
            PropertyPresentationView lView = ViewOf(lHidden);
            CHECK_FALSE(MaterialEditing::Present(lHidden, &k_Fog, lView));
        }

        for (const char* lKept : { "bLit", "NormalMap", "EmissiveColor", "Shader" })
        {
            PropertyPresentationView lView = ViewOf(lKept);
            CHECK(MaterialEditing::Present(lKept, &k_Fog, lView));
            CHECK(lView.Label == lKept);
            CHECK(lView.Tooltip.IsEmpty());
        }
    }

    TEST_CASE("a shader that declares nothing, or none loaded, shows every slot under its own name")
    {
        const ShaderParamList lNothing;
        PropertyPresentationView lView = ViewOf("Param3");
        CHECK(MaterialEditing::Present("Param3", &lNothing, lView));
        CHECK(lView.Label == "Param3");
        CHECK(MaterialEditing::Present("Texture1", nullptr, lView));
    }

    TEST_CASE("a material with no shader hides the values only a shader reads")
    {
        Material2D lMaterial;
        CHECK_FALSE(lMaterial.IsPropertyVisible("Param0"));
        CHECK_FALSE(lMaterial.IsPropertyVisible("Color1"));
        CHECK_FALSE(lMaterial.IsPropertyVisible("Texture0"));
        CHECK(lMaterial.IsPropertyVisible("Shader"));
        CHECK(lMaterial.IsPropertyVisible("bLit"));

        lMaterial.Shader.Path = OpaaxString("Shaders/Fog.glsl");
        CHECK(lMaterial.IsPropertyVisible("Param0"));
        CHECK(lMaterial.IsPropertyVisible("Texture0"));
    }

    TEST_CASE("a newly picked shader gives the material its declared defaults")
    {
        Material2D lMaterial;
        lMaterial.Param0 = 0.9f;
        lMaterial.Param1 = 3.f;

        CHECK(MaterialEditing::ApplyDefaults(lMaterial, k_Fog) == 2u);
        CHECK(lMaterial.Param0 == doctest::Approx(0.5f));
        CHECK(lMaterial.Param6 == doctest::Approx(0.1f));
        CHECK(lMaterial.Param1 == doctest::Approx(3.f));   // not declared: kept

        CHECK(MaterialEditing::ApplyDefaults(lMaterial, k_Fog) == 0u);
        CHECK(&MaterialEditing::NumberOf(lMaterial, EShaderParamSlot::Param6) == &lMaterial.Param6);
    }
}
