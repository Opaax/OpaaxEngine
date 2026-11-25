// Suite: MaterialShaderCache — a material's shader compiled once per file; one that cannot be drawn
// with is logged once and falls back to the engine's sprite shader. A stub stands for the GPU.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Renderer/Materials/MaterialShaderCache.h"
#include "Renderer/Materials/SpriteShader.h"

using namespace Opaax;

namespace
{
    /** A shader that compiled or not, as the stub device says. */
    class StubShader final : public IShader
    {
    public:
        explicit StubShader(const bool bInValid) : m_bValid(bInValid) {}

        void Bind() const override {}
        void Unbind() const override {}
        bool IsValid() const override { return m_bValid; }
        void SetInt(const char*, Int32) override {}
        void SetIntArray(const char*, const Int32*, Uint32) override {}
        void SetFloat(const char*, float) override {}
        void SetFloat2(const char*, const Vector2F&) override {}
        void SetFloat3(const char*, const Vector3F&) override {}
        void SetFloat4(const char*, const Vector4F&) override {}
        void SetMat4(const char*, const Matrix44F&) override {}

    private:
        bool m_bValid = false;
    };

    /** Compiles every shader whose fragment stage does not hold "broken"; counts the compiles. */
    struct StubDevice
    {
        Uint32 Compiles = 0;

        MaterialShaderCache::CreateFunction Function()
        {
            return [this](const ShaderDesc& InDesc) -> TUniquePtr<IShader>
            {
                ++Compiles;
                return MakeUnique<StubShader>(!InDesc.FragmentSrc.Contains("broken"));
            };
        }
    };

    TUniquePtr<SpriteShader> LoadShader(const TestTempDir& InDir, const char* InFile, const char* InFragment)
    {
        const OpaaxString lPath = InDir.Sub(InFile);
        const OpaaxString lText = OpaaxString::Format("#type vertex\nvoid main() {{}}\n#type fragment\n{}\n", InFragment);
        REQUIRE(FileIO::WriteAllText(lPath, lText));

        TUniquePtr<SpriteShader> lShader = MakeUnique<SpriteShader>();
        REQUIRE(lShader->LoadSource(lPath.CStr()));
        return lShader;
    }
}

TEST_SUITE("MaterialShaderCache")
{
    TEST_CASE("a shader is compiled once per file, then shared")
    {
        const TestTempDir  lTemp("material_shader_once");
        const TUniquePtr<SpriteShader> lFog = LoadShader(lTemp, "Fog.glsl", "void main() {}");

        StubDevice          lDevice;
        MaterialShaderCache lCache(lDevice.Function());

        IShader* lFirst = lCache.Resolve(OpaaxString("Shaders/Fog.glsl"), lFog.get());
        REQUIRE(lFirst != nullptr);
        CHECK(lFirst->IsValid());
        CHECK(lCache.Resolve(OpaaxString("Shaders/Fog.glsl"), lFog.get()) == lFirst);
        CHECK(lDevice.Compiles == 1u);

        const TDynArray<MaterialShaderCache::Entry> lEntries = lCache.GetEntries();
        REQUIRE(lEntries.size() == 1u);
        CHECK(lEntries[0].Path == "Shaders/Fog.glsl");
        CHECK(lEntries[0].bValid);
    }

    TEST_CASE("a broken shader falls back, logged and tried once")
    {
        const TestTempDir  lTemp("material_shader_broken");
        const TUniquePtr<SpriteShader> lBroken = LoadShader(lTemp, "Broken.glsl", "broken");
        REQUIRE(FileIO::WriteAllText(lTemp.Sub("Empty.glsl"), OpaaxString("void main() {}\n")));
        SpriteShader lEmpty;
        REQUIRE(lEmpty.LoadSource(lTemp.Sub("Empty.glsl").CStr()));

        StubDevice          lDevice;
        MaterialShaderCache lCache(lDevice.Function());

        // Does not compile: null, every time, compiled once.
        CHECK(lCache.Resolve(OpaaxString("Shaders/Broken.glsl"), lBroken.get()) == nullptr);
        CHECK(lCache.Resolve(OpaaxString("Shaders/Broken.glsl"), lBroken.get()) == nullptr);
        CHECK(lDevice.Compiles == 1u);

        // No sections or no file: null, never compiled.
        CHECK(lCache.Resolve(OpaaxString("Shaders/Empty.glsl"), &lEmpty) == nullptr);
        CHECK(lCache.Resolve(OpaaxString("Shaders/Missing.glsl"), nullptr) == nullptr);
        CHECK(lCache.Resolve(OpaaxString("Shaders/Missing.glsl"), nullptr) == nullptr);
        CHECK(lDevice.Compiles == 1u);

        const TDynArray<MaterialShaderCache::Entry> lEntries = lCache.GetEntries();
        REQUIRE(lEntries.size() == 3u);
        CHECK_FALSE(lEntries[0].bValid);
        CHECK_FALSE(lEntries[1].bValid);
        CHECK_FALSE(lEntries[2].bValid);
    }

    TEST_CASE("clearing hands each compiled shader over before it goes, then compiles again")
    {
        const TestTempDir  lTemp("material_shader_clear");
        const TUniquePtr<SpriteShader> lFog    = LoadShader(lTemp, "Fog.glsl", "void main() {}");
        const TUniquePtr<SpriteShader> lBroken = LoadShader(lTemp, "Broken.glsl", "broken");

        StubDevice          lDevice;
        MaterialShaderCache lCache(lDevice.Function());

        const IShader* lFog1 = lCache.Resolve(OpaaxString("Fog.glsl"), lFog.get());
        lCache.Resolve(OpaaxString("Broken.glsl"), lBroken.get());

        TDynArray<const IShader*> lHanded;
        lCache.Clear([&lHanded](const IShader& InShader) { lHanded.emplace_back(&InShader); });
        REQUIRE(lHanded.size() == 1u);
        CHECK(lHanded[0] == lFog1);
        CHECK(lCache.GetEntries().empty());

        CHECK(lCache.Resolve(OpaaxString("Fog.glsl"), lFog.get()) != nullptr);
        CHECK(lDevice.Compiles == 3u);
    }
}
