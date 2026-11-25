// Suite: SpriteShader — a game's sprite shader read from its .glsl file. Text only: compiling it needs
// a GPU, which the TestWorld MaterialShader script has.
#include <doctest.h>

#include "../TestFolderPaths.h"
#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/Materials/SpriteShader.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/ResourceFormat.h"
#include "Resources/ResourceManager.h"

using namespace Opaax;

namespace
{
    constexpr const char* k_TwoStages =
        "// a comment before the first section is ignored\n"
        "#type vertex\n"
        "#version 450 core\n"
        "void main() { gl_Position = vec4(0.0); }\n"
        "#type fragment\n"
        "#version 450 core\n"
        "layout(location = 0) out vec4 FragColor;\n"
        "void main() { FragColor = vec4(1.0); }\n";
}

TEST_SUITE("SpriteShader")
{
    TEST_CASE("a file with both sections loads split into its stages, named after the file")
    {
        const TestTempDir lTemp("spriteshader_split");
        const OpaaxString lPath = lTemp.Sub("Fog.glsl");
        REQUIRE(FileIO::WriteAllText(lPath, OpaaxString(k_TwoStages)));

        SpriteShader lShader;
        REQUIRE(lShader.LoadSource(lPath.CStr()));

        CHECK(lShader.HasStages());
        CHECK(lShader.GetDesc().DebugName == lPath);
        CHECK(lShader.GetDesc().VertexSrc.StartsWith("#version 450 core\n"));
        CHECK(lShader.GetDesc().VertexSrc.Contains("gl_Position"));
        CHECK_FALSE(lShader.GetDesc().VertexSrc.Contains("FragColor"));
        CHECK(lShader.GetDesc().FragmentSrc.Contains("FragColor = vec4(1.0)"));
        CHECK_FALSE(lShader.GetDesc().FragmentSrc.Contains("comment"));
        CHECK(lShader.GetResourceSize() > sizeof(SpriteShader));
    }

    TEST_CASE("a copy of the engine's sprite shader is a whole game shader")
    {
        SpriteShader lShader;
        const OpaaxString lPath = OpaaxString::Format("{}/Shaders/Sprite.glsl", OPAAX_TEST_ENGINE_ASSETS);
        REQUIRE(lShader.LoadSource(lPath.CStr()));
        CHECK(lShader.HasStages());
    }

    TEST_CASE("a file without both sections loads empty: its sprites fall back to the engine's shader")
    {
        const TestTempDir lTemp("spriteshader_empty");

        for (const char* lText : { "", "void main() {}\n", "#type vertex\nvoid main() {}\n" })
        {
            const OpaaxString lPath = lTemp.Sub("Broken.glsl");
            REQUIRE(FileIO::WriteAllText(lPath, OpaaxString(lText)));

            SpriteShader lShader;
            CHECK(lShader.LoadSource(lPath.CStr()));
            CHECK_FALSE(lShader.HasStages());
            CHECK(lShader.GetDesc().DebugName == lPath);
        }
    }

    TEST_CASE("no file is a failed load")
    {
        const TestTempDir lTemp("spriteshader_missing");

        SpriteShader lShader;
        CHECK_FALSE(lShader.LoadSource(lTemp.Sub("Missing.glsl").CStr()));
        CHECK_FALSE(lShader.HasStages());
    }

    TEST_CASE("is read from .glsl files and loads through the resource manager")
    {
        ObjectClassRegistry lClasses;
        REQUIRE(lClasses.Register<SpriteShader>());
        CHECK(lClasses.FindBySourceExtension(NormalizeExtension(".glsl")) == &SpriteShader::StaticClass());
        CHECK(lClasses.FindBySourceExtension(NormalizeExtension(".GLSL")) == &SpriteShader::StaticClass());

        const TestTempDir lTemp("spriteshader_asset");
        REQUIRE(FileIO::WriteAllText(lTemp.Sub("Fog.glsl"), OpaaxString(k_TwoStages)));

        ResourceManager lResources;
        lResources.SetObjectClasses(&lClasses);
        REQUIRE(lResources.Startup());

        const TestFolderPaths lPaths(lTemp.Path());
        {
            const TAssetHandle<SpriteShader> lHandle =
                LoadAsset<SpriteShader>(lResources, lPaths, OpaaxString("Fog.glsl"));

            const SpriteShader* lShader = lHandle.Get();
            REQUIRE(lShader != nullptr);
            CHECK(lShader->GetName() == OPAAX_ID("Fog"));
            CHECK(lShader->HasStages());
        }

        lResources.Shutdown();
    }
}
