// Suite: ShaderParams — the `// @param` lines of a game's sprite shader: which material values it
// reads, under what names, with what tooltip, range and default. Text only.
#include <doctest.h>

#include "Renderer/Materials/ShaderParams.h"

using namespace Opaax;

TEST_SUITE("ShaderParams")
{
    TEST_CASE("each form of a declaration is read, in order, anywhere in the file")
    {
        const ShaderParamList lList = ShaderParams::Parse(
            "// The fog of the help map.\n"
            "// @param Param0   EdgeCut   \"Where the fog is cut\"   range(0, 1) default(0.5)\n"
            "#type vertex\n"
            "void main() {}\n"
            "#type fragment\n"
            "  //@param Color0 ChalkTint \"Tint of the grain\"\r\n"
            "// @param Texture1 Noise\n"
            "// @param Param7 Drift default(-0.25) range(-2,2)\n");

        CHECK(lList.Errors.empty());
        REQUIRE(lList.Params.size() == 4u);

        const ShaderParam& lCut = lList.Params[0];
        CHECK(lCut.Slot == EShaderParamSlot::Param0);
        CHECK(lCut.Name == "EdgeCut");
        CHECK(lCut.Tooltip == "Where the fog is cut");
        CHECK(lCut.bHasRange);
        CHECK(lCut.RangeMin == doctest::Approx(0.f));
        CHECK(lCut.RangeMax == doctest::Approx(1.f));
        CHECK(lCut.bHasDefault);
        CHECK(lCut.Default == doctest::Approx(0.5f));

        CHECK(lList.Params[1].Slot == EShaderParamSlot::Color0);
        CHECK(lList.Params[1].Name == "ChalkTint");
        CHECK(lList.Params[1].Tooltip == "Tint of the grain");
        CHECK_FALSE(lList.Params[1].bHasRange);

        CHECK(lList.Params[2].Slot == EShaderParamSlot::Texture1);
        CHECK(lList.Params[2].Tooltip.IsEmpty());

        CHECK(lList.Params[3].Slot == EShaderParamSlot::Param7);
        CHECK(lList.Params[3].Default == doctest::Approx(-0.25f));
        CHECK(lList.Params[3].RangeMin == doctest::Approx(-2.f));

        CHECK(lList.Find(EShaderParamSlot::Color0) == &lList.Params[1]);
        CHECK(lList.Find(EShaderParamSlot::Param1) == nullptr);
    }

    TEST_CASE("a shader that declares nothing has no params and no errors")
    {
        const ShaderParamList lList = ShaderParams::Parse(
            "#type vertex\nvoid main() {}\n// a comment about @parameters\n// @paramless\n");
        CHECK(lList.Params.empty());
        CHECK(lList.Errors.empty());
    }

    TEST_CASE("a refused line is left out, with its line and its reason")
    {
        const ShaderParamList lList = ShaderParams::Parse(
            "// @param Param9 Nine\n"                         // 1: unknown slot
            "// @param Param0 9lives\n"                       // 2: starts with a digit
            "// @param Param0 u_Time\n"                       // 3: the engine's prefix
            "// @param Param0 Cut \"no end\n"                 // 4: tooltip not closed
            "// @param Color0 Tint range(0, 1)\n"             // 5: range on a colour
            "// @param Param0 Cut range(1, 0)\n"              // 6: range the wrong way
            "// @param Param0 Cut default()\n"                // 7: default without a number
            "// @param Param0 Cut speed(2)\n"                 // 8: unknown option
            "// @param Param0 Cut\n"                          // 9: kept
            "// @param Param0 Again\n"                        // 10: slot twice
            "// @param Param1 Cut\n");                        // 11: name twice

        REQUIRE(lList.Params.size() == 1u);
        CHECK(lList.Params[0].Name == "Cut");

        REQUIRE(lList.Errors.size() == 10u);
        CHECK(lList.Errors[0] == "line 1: unknown slot 'Param9' (Param0..7, Color0..1, Texture0..1)");
        CHECK(lList.Errors[1].StartsWith("line 2: '9lives' is not a name the shader can use"));
        CHECK(lList.Errors[2].StartsWith("line 3: 'u_Time' is not a name the shader can use"));
        CHECK(lList.Errors[3] == "line 4: the tooltip has no closing '\"'");
        CHECK(lList.Errors[4] == "line 5: range() is for Param0..7, not Color0");
        CHECK(lList.Errors[5].StartsWith("line 6: range() takes two numbers"));
        CHECK(lList.Errors[6].StartsWith("line 7: default() takes one number"));
        CHECK(lList.Errors[7] == "line 8: unknown option 'speed' (range, default)");
        CHECK(lList.Errors[8] == "line 10: Param0 is declared twice");
        CHECK(lList.Errors[9] == "line 11: the name 'Cut' is used twice");
    }

    TEST_CASE("each slot has its field name and the expression the shader reads it by")
    {
        EShaderParamSlot lSlot = EShaderParamSlot::Count;
        CHECK(ShaderParams::SlotFromName("Param5", lSlot));
        CHECK(lSlot == EShaderParamSlot::Param5);
        CHECK_FALSE(ShaderParams::SlotFromName("param5", lSlot));

        CHECK(OpaaxStringView(ShaderParams::SlotName(EShaderParamSlot::Texture1)) == OpaaxStringView("Texture1"));
        CHECK(ShaderParams::IsNumber(EShaderParamSlot::Param7));
        CHECK_FALSE(ShaderParams::IsNumber(EShaderParamSlot::Color0));

        CHECK(ShaderParams::Expression(EShaderParamSlot::Param0) == "u_Params[0].x");
        CHECK(ShaderParams::Expression(EShaderParamSlot::Param5) == "u_Params[1].y");
        CHECK(ShaderParams::Expression(EShaderParamSlot::Param7) == "u_Params[1].w");
        CHECK(ShaderParams::Expression(EShaderParamSlot::Color1) == "u_Colors[1]");
        CHECK(ShaderParams::Expression(EShaderParamSlot::Texture0) == "u_Textures[MATERIAL_SLOT_0]");
    }

    TEST_CASE("the declared names are defined right after the #version line")
    {
        const ShaderParamList lParams = ShaderParams::Parse(
            "// @param Param0 EdgeCut\n// @param Color1 Ink\n// @param Texture0 Noise\n");

        const OpaaxString lStage("// a comment first\n#version 450 core\nvoid main() { float a = EdgeCut; }\n");
        const OpaaxString lOut = ShaderParams::InsertDefines(lStage, lParams);

        CHECK(lOut.StartsWith("// a comment first\n#version 450 core\n// The names of the shader's @param lines.\n"));
        CHECK(lOut.Contains("#define EdgeCut u_Params[0].x\n"));
        CHECK(lOut.Contains("#define Ink u_Colors[1]\n"));
        CHECK(lOut.Contains("#define Noise u_Textures[MATERIAL_SLOT_0]\n"));
        CHECK(lOut.Find("#define Noise") < lOut.Find("void main()"));
        CHECK(lOut.Contains("void main() { float a = EdgeCut; }\n"));

        // No #version line: on top. Nothing declared: unchanged.
        CHECK(ShaderParams::InsertDefines(OpaaxString("void main() {}\n"), lParams).StartsWith("// The names"));
        CHECK(ShaderParams::InsertDefines(lStage, ShaderParamList{}) == lStage);
    }
}
