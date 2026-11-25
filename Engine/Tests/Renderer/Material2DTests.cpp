// Suite: Material2D with a game's shader — the shader file it names, its parameters, colours and
// textures, written to an .opaaxasset and read back through the resource manager.
#include <doctest.h>

#include "../TestFolderPaths.h"
#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/Materials/Material2D.h"
#include "Renderer/Materials/MaterialBlock.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/ResourceManager.h"

using namespace Opaax;

namespace
{
    constexpr const char* k_FogShader =
        "#type vertex\n"
        "#version 450 core\n"
        "void main() { gl_Position = vec4(0.0); }\n"
        "#type fragment\n"
        "#version 450 core\n"
        "layout(location = 0) out vec4 FragColor;\n"
        "void main() { FragColor = vec4(1.0); }\n";
}

TEST_SUITE("Material2D")
{
    TEST_CASE("a material with no shader keeps the engine's: every shader field at its default")
    {
        const Material2D lMaterial;
        CHECK(lMaterial.Shader.IsEmpty());
        CHECK(lMaterial.Param0 == 0.f);
        CHECK(lMaterial.Param7 == 0.f);
        CHECK(lMaterial.Color0.r == 1.f);
        CHECK(lMaterial.Color1.a == 1.f);
        CHECK(lMaterial.Texture0.IsEmpty());
        CHECK(lMaterial.Texture1.IsEmpty());

        // A file written before shaders still reads: the missing fields keep their defaults.
        Material2D lOld;
        REQUIRE(Material2D::StaticClass().ReadProperties(lOld, nlohmann::json{ { "bLit", false } }));
        CHECK_FALSE(lOld.bLit);
        CHECK(lOld.Shader.IsEmpty());
        CHECK(lOld.Color0.g == 1.f);
    }

    TEST_CASE("a material naming a shader file saves and loads back with its parameters")
    {
        const TestTempDir lTemp("material2d_shader");
        REQUIRE(FileIO::WriteAllText(lTemp.Sub("Shaders/Fog.glsl"), OpaaxString(k_FogShader)));

        Material2D lMaterial;
        lMaterial.Shader.Path   = OpaaxString("Shaders/Fog.glsl");
        lMaterial.Param0        = 0.5f;
        lMaterial.Param3        = -2.f;
        lMaterial.Param5        = 1.25f;
        lMaterial.Param7        = 7.f;
        lMaterial.Color0        = LinearColor{ 0.2f, 0.4f, 0.6f, 0.8f };
        lMaterial.Color1        = LinearColor{ 1.f, 0.f, 0.f, 1.f };
        lMaterial.Texture0.Path = OpaaxString("Textures/ChalkNoise.png");
        lMaterial.Texture1.Path = OpaaxString("/Runtime/HelpMap/Fog");
        REQUIRE(AssetFile::Save(lTemp.Sub("Materials/Fog.opaaxasset"), lMaterial));

        ObjectClassRegistry lClasses;
        REQUIRE(lClasses.Register<Texture2D>());
        REQUIRE(lClasses.Register<SpriteShader>());
        REQUIRE(lClasses.Register<Material2D>());

        ResourceManager lResources;
        lResources.SetObjectClasses(&lClasses);
        REQUIRE(lResources.Startup());

        const TestFolderPaths lPaths(lTemp.Path());
        {
            const TAssetHandle<Material2D> lHandle =
                LoadAsset<Material2D>(lResources, lPaths, OpaaxString("Materials/Fog.opaaxasset"));

            const Material2D* lBack = lHandle.Get();
            REQUIRE(lBack != nullptr);
            CHECK(lBack->Shader.Path == "Shaders/Fog.glsl");
            CHECK(lBack->Param0 == doctest::Approx(0.5f));
            CHECK(lBack->Param1 == doctest::Approx(0.f));
            CHECK(lBack->Param3 == doctest::Approx(-2.f));
            CHECK(lBack->Param5 == doctest::Approx(1.25f));
            CHECK(lBack->Param7 == doctest::Approx(7.f));
            CHECK(lBack->Color0.g == doctest::Approx(0.4f));
            CHECK(lBack->Color0.a == doctest::Approx(0.8f));
            CHECK(lBack->Color1.r == doctest::Approx(1.f));
            CHECK(lBack->Texture0.Path == "Textures/ChalkNoise.png");
            CHECK(lBack->Texture1.Path == "/Runtime/HelpMap/Fog");

            // The shader it names is the file, read as a SpriteShader.
            const TAssetHandle<SpriteShader> lShader = LoadAsset<SpriteShader>(lResources, lPaths, lBack->Shader.Path);
            REQUIRE(lShader.Get() != nullptr);
            CHECK(lShader.Get()->HasStages());
            CHECK(lShader.Get()->GetDesc().FragmentSrc.Contains("FragColor = vec4(1.0)"));
        }

        lResources.Shutdown();
    }

    TEST_CASE("the material block lays out the times, the eight parameters and the two colours")
    {
        Material2D lMaterial;
        lMaterial.Param0 = 1.f;
        lMaterial.Param3 = 4.f;
        lMaterial.Param4 = 5.f;
        lMaterial.Param7 = 8.f;
        lMaterial.Color0 = LinearColor{ 0.1f, 0.2f, 0.3f, 0.4f };
        lMaterial.Color1 = LinearColor{ 0.5f, 0.6f, 0.7f, 0.8f };

        const MaterialBlock lBlock = MakeMaterialBlock(lMaterial, 12.5, 100.25);
        CHECK(lBlock.Time.x == doctest::Approx(12.5f));
        CHECK(lBlock.Time.y == doctest::Approx(100.25f));
        CHECK(lBlock.Params[0].x == doctest::Approx(1.f));
        CHECK(lBlock.Params[0].w == doctest::Approx(4.f));
        CHECK(lBlock.Params[1].x == doctest::Approx(5.f));
        CHECK(lBlock.Params[1].w == doctest::Approx(8.f));
        CHECK(lBlock.Colors[0].y == doctest::Approx(0.2f));
        CHECK(lBlock.Colors[1].w == doctest::Approx(0.8f));
        CHECK(sizeof(MaterialBlock) == 80u);
    }
}
