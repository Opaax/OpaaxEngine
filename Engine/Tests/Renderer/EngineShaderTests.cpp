// Suite: the engine's shader files (Engine/Assets/Shaders). Each splits into its two stages and
// ports to GLSL 4.10 with the bindings the C++ side binds, and the constants a shader repeats
// from C++ (light counts, the shadow map's size and slot) match. Text only: compiling them needs
// a GPU, which the CI render check has.
#include <doctest.h>

#include <regex>
#include <string>

#include "Core/IO/FileIO.h"
#include "Renderer/Lighting/Lighting2D.h"
#include "Renderer/ShaderSource.h"
#include "RHI/OpenGL/GLSLPort.h"

using namespace Opaax;

namespace
{
    std::string ReadShader(const char* InFile)
    {
        const std::string lPath = std::string(OPAAX_TEST_ENGINE_ASSETS) + "/Shaders/" + InFile;
        return std::string(FileIO::ReadAllText(OpaaxString(lPath.c_str())).CStr());
    }

    /** The bindings a stage declares, once ported. Fails the test when a `binding = N` is left behind. */
    TDynArray<GLSLPort::ResourceBinding> PortStage(const OpaaxString& InStage)
    {
        TDynArray<GLSLPort::ResourceBinding> lBindings;
        const std::string lPorted = GLSLPort::To410(std::string(InStage.CStr()), lBindings);

        CHECK(lPorted.rfind("#version 410 core\n", 0) == 0);
        CHECK_FALSE(std::regex_search(lPorted, std::regex("binding\\s*=")));
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

    /** The value of `const <type> InName = <value>;` in InSource, or -1. */
    double ConstantIn(const std::string& InSource, const char* InName)
    {
        const std::regex lPattern(std::string("const\\s+(int|float)\\s+") + InName + "\\s*=\\s*([0-9.]+)\\s*;");
        std::smatch      lMatch;
        return std::regex_search(InSource, lMatch, lPattern) ? std::stod(lMatch[2].str()) : -1.0;
    }
}

TEST_CASE("Engine shaders: the sprite shader ports with its samplers, camera and lights")
{
    const std::string lSource = ReadShader("Sprite.glsl");
    REQUIRE_FALSE(lSource.empty());

    const ShaderDesc lDesc = ShaderSource::FromSource(OpaaxString(lSource.c_str()), OpaaxString("Sprite.glsl"));
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
    const std::string lSource = ReadShader("Sprite.glsl");

    CHECK(ConstantIn(lSource, "MAX_LIGHTS") == doctest::Approx(MAX_LIGHTS_2D));
    CHECK(ConstantIn(lSource, "SHADOW_ROWS") == doctest::Approx(MAX_SHADOWED_LIGHTS_2D));
    CHECK(ConstantIn(lSource, "SHADOW_ANGLES") == doctest::Approx(SHADOW_MAP_ANGLES_2D));

    // The lighting maps ride in the last two of the 16 samplers (Renderer2D's AO_MAP_SLOT, SHADOW_MAP_SLOT).
    CHECK(ConstantIn(lSource, "AO_SLOT") == doctest::Approx(14));
    CHECK(ConstantIn(lSource, "SHADOW_SLOT") == doctest::Approx(15));
}

TEST_CASE("Engine shaders: the ambient occlusion shader ports with its source and block")
{
    const std::string lSource = ReadShader("AmbientOcclusion2D.glsl");
    REQUIRE_FALSE(lSource.empty());

    const ShaderDesc lDesc = ShaderSource::FromSource(OpaaxString(lSource.c_str()), OpaaxString("AmbientOcclusion2D.glsl"));
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
    const std::string lSource = ReadShader("Shadow2D.glsl");
    REQUIRE_FALSE(lSource.empty());

    const ShaderDesc lDesc = ShaderSource::FromSource(OpaaxString(lSource.c_str()), OpaaxString("Shadow2D.glsl"));
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
    const std::string lSource = ReadShader("Tonemap.glsl");
    REQUIRE_FALSE(lSource.empty());

    const ShaderDesc lDesc = ShaderSource::FromSource(OpaaxString(lSource.c_str()), OpaaxString("Tonemap.glsl"));
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
    const std::string lSource = ReadShader("Bloom2D.glsl");
    REQUIRE_FALSE(lSource.empty());

    const ShaderDesc lDesc = ShaderSource::FromSource(OpaaxString(lSource.c_str()), OpaaxString("Bloom2D.glsl"));
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
