// Suite: the engine's shader files (Engine/Assets/Shaders). Each splits into its two stages and
// ports to GLSL 4.10 with the bindings the C++ side binds, and the constants a shader repeats
// from C++ (light counts, the shadow map's size and slot) match. Text only: compiling them needs
// a GPU, which the CI render check has.
#include <doctest.h>

#include "Core/IO/FileIO.h"
#include "Core/String/Char.h"
#include "Core/String/CString.h"
#include "Renderer/Lighting/Lighting2D.h"
#include "Renderer/ShaderSource.h"
#include "RHI/OpenGL/GLSLPort.h"

using namespace Opaax;

namespace
{
    OpaaxString ReadShader(const char* InFile)
    {
        return FileIO::ReadAllText(OpaaxString::Format("{}/Shaders/{}", OPAAX_TEST_ENGINE_ASSETS, InFile));
    }

    /** Moves past the spaces at the start of InOutText; false when there were fewer than InMinimum. */
    bool SkipSpaces(OpaaxStringView& InOutText, const Uint32 InMinimum)
    {
        const OpaaxStringView lTrimmed = InOutText.TrimStart();
        const Uint32          lSkipped = InOutText.GetLength() - lTrimmed.GetLength();
        InOutText = lTrimmed;
        return lSkipped >= InMinimum;
    }

    /** Moves past InWord when InOutText starts with it. */
    bool Consume(OpaaxStringView& InOutText, const OpaaxStringView InWord)
    {
        if (!InOutText.StartsWith(InWord)) { return false; }

        InOutText.RemovePrefix(InWord.GetLength());
        return true;
    }

    /** InText holds a `binding` followed by spaces and `=`. */
    bool HasBindingAssignment(const OpaaxStringView InText)
    {
        for (Int32 lAt = InText.Find("binding"); lAt >= 0; lAt = InText.Find("binding", static_cast<Uint32>(lAt) + 1))
        {
            OpaaxStringView lAfter = InText.SubString(static_cast<Uint32>(lAt) + 7);
            SkipSpaces(lAfter, 0);
            if (lAfter.StartsWith("=")) { return true; }
        }
        return false;
    }

    /** The bindings a stage declares, once ported. Fails the test when a `binding = N` is left behind. */
    TDynArray<GLSLPort::ResourceBinding> PortStage(const OpaaxString& InStage)
    {
        TDynArray<GLSLPort::ResourceBinding> lBindings;
        const OpaaxString lPorted = GLSLPort::To410(InStage, lBindings);

        CHECK(lPorted.StartsWith("#version 410 core\n"));
        CHECK_FALSE(HasBindingAssignment(lPorted));
        return lBindings;
    }

    const GLSLPort::ResourceBinding* Find(const TDynArray<GLSLPort::ResourceBinding>& InBindings, const char* InName)
    {
        for (const GLSLPort::ResourceBinding& lBinding : InBindings)
        {
            if (lBinding.Name == InName) { return &lBinding; }
        }
        return nullptr;
    }

    /** The value of `const <int or float> InName = <digits and dots>;` in InSource, or -1. */
    double ConstantIn(const OpaaxStringView InSource, const char* InName)
    {
        for (Int32 lAt = InSource.Find("const"); lAt >= 0; lAt = InSource.Find("const", static_cast<Uint32>(lAt) + 1))
        {
            OpaaxStringView lText = InSource.SubString(static_cast<Uint32>(lAt) + 5);
            if (!SkipSpaces(lText, 1) || !(Consume(lText, "int") || Consume(lText, "float"))
                || !SkipSpaces(lText, 1) || !Consume(lText, InName))
            {
                continue;
            }

            SkipSpaces(lText, 0);
            if (!Consume(lText, "=")) { continue; }
            SkipSpaces(lText, 0);

            Uint32 lDigits = 0;
            while (lDigits < lText.GetLength() && (Char::IsDigit(lText[lDigits]) || lText[lDigits] == '.')) { ++lDigits; }

            OpaaxStringView lAfter = lText.SubString(lDigits);
            SkipSpaces(lAfter, 0);
            if (lDigits == 0 || !lAfter.StartsWith(";")) { continue; }

            return CString::ToDouble(lText.SubString(0, lDigits).ToString().CStr());
        }
        return -1.0;
    }
}

TEST_CASE("Engine shaders: the checks find what they look for")
{
    CHECK(HasBindingAssignment("layout(binding = 2) uniform Block"));
    CHECK(HasBindingAssignment("binding=1"));
    CHECK_FALSE(HasBindingAssignment("layout(bindings = 2) uniform Block"));
    CHECK_FALSE(HasBindingAssignment("binding"));

    CHECK(ConstantIn("const int MAX = 32;", "MAX") == doctest::Approx(32));
    CHECK(ConstantIn("const  float  K=0.5 ;", "K") == doctest::Approx(0.5));
    CHECK(ConstantIn("const int MAX_X = 1;", "MAX") == doctest::Approx(-1));
    CHECK(ConstantIn("constint MAX = 3;", "MAX") == doctest::Approx(-1));
}

TEST_CASE("Engine shaders: the sprite shader ports with its samplers, camera and lights")
{
    const OpaaxString lSource = ReadShader("Sprite.glsl");
    REQUIRE_FALSE(lSource.IsEmpty());

    const ShaderDesc lDesc = ShaderSource::FromSource(lSource, OpaaxString("Sprite.glsl"));
    REQUIRE_FALSE(lDesc.VertexSrc.IsEmpty());
    REQUIRE_FALSE(lDesc.FragmentSrc.IsEmpty());

    PortStage(lDesc.VertexSrc);
    const TDynArray<GLSLPort::ResourceBinding> lBindings = PortStage(lDesc.FragmentSrc);

    const GLSLPort::ResourceBinding* lTextures = Find(lBindings, "u_Textures");
    REQUIRE(lTextures != nullptr);
    CHECK(lTextures->Binding == 0);
    CHECK(lTextures->ArraySize == 16);

    const GLSLPort::ResourceBinding* lCamera = Find(lBindings, "CameraUBO");
    REQUIRE(lCamera != nullptr);
    CHECK(lCamera->Binding == 1);

    const GLSLPort::ResourceBinding* lLights = Find(lBindings, "LightsUBO");
    REQUIRE(lLights != nullptr);
    CHECK(lLights->Binding == 3);
}

TEST_CASE("Engine shaders: the sprite shader's lighting constants match Lighting2D")
{
    const OpaaxString lSource = ReadShader("Sprite.glsl");

    CHECK(ConstantIn(lSource, "MAX_LIGHTS") == doctest::Approx(MAX_LIGHTS_2D));
    CHECK(ConstantIn(lSource, "SHADOW_ROWS") == doctest::Approx(MAX_SHADOWED_LIGHTS_2D));
    CHECK(ConstantIn(lSource, "SHADOW_ANGLES") == doctest::Approx(SHADOW_MAP_ANGLES_2D));

    // The lighting maps ride in the last two of the 16 samplers (Renderer2D's AO_MAP_SLOT, SHADOW_MAP_SLOT).
    CHECK(ConstantIn(lSource, "AO_SLOT") == doctest::Approx(14));
    CHECK(ConstantIn(lSource, "SHADOW_SLOT") == doctest::Approx(15));
}

TEST_CASE("Engine shaders: the ambient occlusion shader ports with its source and block")
{
    const OpaaxString lSource = ReadShader("AmbientOcclusion2D.glsl");
    REQUIRE_FALSE(lSource.IsEmpty());

    const ShaderDesc lDesc = ShaderSource::FromSource(lSource, OpaaxString("AmbientOcclusion2D.glsl"));
    REQUIRE_FALSE(lDesc.VertexSrc.IsEmpty());
    REQUIRE_FALSE(lDesc.FragmentSrc.IsEmpty());

    PortStage(lDesc.VertexSrc);
    const TDynArray<GLSLPort::ResourceBinding> lBindings = PortStage(lDesc.FragmentSrc);

    const GLSLPort::ResourceBinding* lInput = Find(lBindings, "u_Source");
    REQUIRE(lInput != nullptr);
    CHECK(lInput->Binding == 0);

    const GLSLPort::ResourceBinding* lBlock = Find(lBindings, "AmbientOcclusionUBO");
    REQUIRE(lBlock != nullptr);
    CHECK(lBlock->Binding == 5);

    CHECK(ConstantIn(lSource, "DOWNSAMPLE") == doctest::Approx(AO_DOWNSAMPLE_2D));
}

TEST_CASE("Engine shaders: the shadow map shader ports with its occlusion map and block")
{
    const OpaaxString lSource = ReadShader("Shadow2D.glsl");
    REQUIRE_FALSE(lSource.IsEmpty());

    const ShaderDesc lDesc = ShaderSource::FromSource(lSource, OpaaxString("Shadow2D.glsl"));
    REQUIRE_FALSE(lDesc.VertexSrc.IsEmpty());
    REQUIRE_FALSE(lDesc.FragmentSrc.IsEmpty());

    PortStage(lDesc.VertexSrc);
    const TDynArray<GLSLPort::ResourceBinding> lBindings = PortStage(lDesc.FragmentSrc);

    const GLSLPort::ResourceBinding* lOcclusion = Find(lBindings, "u_Occlusion");
    REQUIRE(lOcclusion != nullptr);
    CHECK(lOcclusion->Binding == 0);

    const GLSLPort::ResourceBinding* lBlock = Find(lBindings, "ShadowUBO");
    REQUIRE(lBlock != nullptr);
    CHECK(lBlock->Binding == 4);

    CHECK(ConstantIn(lSource, "MAX_SHADOWED") == doctest::Approx(MAX_SHADOWED_LIGHTS_2D));
}

TEST_CASE("Engine shaders: the tonemap shader ports with its scene and post block")
{
    const OpaaxString lSource = ReadShader("Tonemap.glsl");
    REQUIRE_FALSE(lSource.IsEmpty());

    const ShaderDesc lDesc = ShaderSource::FromSource(lSource, OpaaxString("Tonemap.glsl"));
    REQUIRE_FALSE(lDesc.VertexSrc.IsEmpty());
    REQUIRE_FALSE(lDesc.FragmentSrc.IsEmpty());

    PortStage(lDesc.VertexSrc);
    const TDynArray<GLSLPort::ResourceBinding> lBindings = PortStage(lDesc.FragmentSrc);

    const GLSLPort::ResourceBinding* lScene = Find(lBindings, "u_Scene");
    REQUIRE(lScene != nullptr);
    CHECK(lScene->Binding == 0);

    const GLSLPort::ResourceBinding* lBloom = Find(lBindings, "u_Bloom");
    REQUIRE(lBloom != nullptr);
    CHECK(lBloom->Binding == 1);

    const GLSLPort::ResourceBinding* lPost = Find(lBindings, "PostUBO");
    REQUIRE(lPost != nullptr);
    CHECK(lPost->Binding == 2);
}

TEST_CASE("Engine shaders: the bloom shader ports with its source and block")
{
    const OpaaxString lSource = ReadShader("Bloom2D.glsl");
    REQUIRE_FALSE(lSource.IsEmpty());

    const ShaderDesc lDesc = ShaderSource::FromSource(lSource, OpaaxString("Bloom2D.glsl"));
    REQUIRE_FALSE(lDesc.VertexSrc.IsEmpty());
    REQUIRE_FALSE(lDesc.FragmentSrc.IsEmpty());

    PortStage(lDesc.VertexSrc);
    const TDynArray<GLSLPort::ResourceBinding> lBindings = PortStage(lDesc.FragmentSrc);

    const GLSLPort::ResourceBinding* lInput = Find(lBindings, "u_Source");
    REQUIRE(lInput != nullptr);
    CHECK(lInput->Binding == 0);

    const GLSLPort::ResourceBinding* lBlock = Find(lBindings, "BloomUBO");
    REQUIRE(lBlock != nullptr);
    CHECK(lBlock->Binding == 6);
}
