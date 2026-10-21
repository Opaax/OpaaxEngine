// Suite: Texture2D — an image file as an asset. Reading (file IO + decode) is tested here; the GPU
// upload needs a GL context and is covered by the feature tests. Without a device, the upload frees
// the decoded data instead of crashing.
//
// The fixture PNG is 4x2 with a red top row and a blue bottom row: 4 != 2 catches swapped
// width/height, the colours catch a missing vertical flip.
// File cases use a unique temp directory, created and removed per case.
#include <doctest.h>

#include "../TestFolderPaths.h"
#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Core/Memory/Memory.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/Textures/Texture2D.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/ResourceFormat.h"
#include "Resources/ResourceManager.h"

using namespace Opaax;

namespace
{
    // 4x2 RGB PNG: top row red, bottom row blue. Stored as bytes (no image file needed).
    constexpr Uint8 k_Png4x2Rgb[] =
    {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0xF0, 0xCA, 0xEA, 0x34, 0x00, 0x00, 0x00,
        0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
        0x47, 0x0C, 0x48, 0xEC, 0xFF, 0x00, 0x67, 0xB2, 0x07, 0xF9, 0x5B, 0xD7,
        0x7C, 0x91, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42,
        0x60, 0x82,
    };

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag) : m_Dir(InTag) {}

        const OpaaxString& Path() const noexcept { return m_Dir.Path(); }

        OpaaxString Sub(const char* InRel) const { return m_Dir.Sub(InRel); }

        /** Write raw bytes to InRel and answer its absolute path. */
        OpaaxString Write(const char* InRel, const Uint8* InBytes, Uint64 InCount) const
        {
            const OpaaxString lPath = Sub(InRel);
            FileIO::WriteAllBytes(lPath, InBytes, InCount);
            return lPath;
        }

    private:
        TestTempDir m_Dir;
    };

    /** A PNG whose signature passes any sniff, and whose data does not decode. */
    TDynArray<Uint8> BrokenPng()
    {
        TDynArray<Uint8> lBroken(k_Png4x2Rgb, k_Png4x2Rgb + sizeof(k_Png4x2Rgb));
        lBroken[30] = 0x00;
        lBroken[45] = 0x7F;
        return lBroken;
    }
}

TEST_SUITE("Texture2D")
{
    TEST_CASE("decodes dimensions and channels from a real PNG")
    {
        const ScopedTempDir lTemp("decode");
        const OpaaxString   lPath = lTemp.Write("Sprite.png", k_Png4x2Rgb, sizeof(k_Png4x2Rgb));

        Texture2D lTexture;
        REQUIRE(lTexture.LoadSource(lPath.CStr()));

        CHECK(lTexture.GetWidth()    == 4u);
        CHECK(lTexture.GetHeight()   == 2u);
        CHECK(lTexture.GetChannels() == 3);
        CHECK(lTexture.GetPixels().size() == 4u * 2u * 3u);

        // From the dimensions, so it is the same before and after the upload frees the pixels.
        CHECK(lTexture.GetResourceSize() == sizeof(Texture2D) + 24u);
    }

    TEST_CASE("decoded rows are FLIPPED for GL's bottom-up sampling")
    {
        const ScopedTempDir lTemp("flip");
        const OpaaxString   lPath = lTemp.Write("Sprite.png", k_Png4x2Rgb, sizeof(k_Png4x2Rgb));

        Texture2D lTexture;
        REQUIRE(lTexture.LoadSource(lPath.CStr()));
        REQUIRE(lTexture.GetPixels().size() >= 3u);

        // The file's FIRST row is red; after the flip the buffer's first row must be the file's
        // LAST row, which is blue. Without the flip every sprite draws upside down and nothing
        // else in the engine would ever report it.
        CHECK(lTexture.GetPixels()[0] == 0u);
        CHECK(lTexture.GetPixels()[1] == 0u);
        CHECK(lTexture.GetPixels()[2] == 255u);
    }

    TEST_CASE("a file that is not an image, or no file, is refused")
    {
        const ScopedTempDir    lTemp("corrupt");
        const TDynArray<Uint8> lBroken = BrokenPng();
        const OpaaxString      lPath   = lTemp.Write("Broken.png", lBroken.data(), lBroken.size());

        Texture2D lTexture;
        CHECK_FALSE(lTexture.LoadSource(lPath.CStr()));
        CHECK_FALSE(lTexture.LoadSource(lTemp.Sub("Nope.png").CStr()));
    }

    TEST_CASE("the upload with no device frees the pixels and keeps the dimensions")
    {
        const ScopedTempDir lTemp("no_device");
        const OpaaxString   lPath = lTemp.Write("Sprite.png", k_Png4x2Rgb, sizeof(k_Png4x2Rgb));

        Texture2D lTexture;
        REQUIRE(lTexture.LoadSource(lPath.CStr()));

        lTexture.InitializeOnMainThread();   // IEngine::Null(): no device

        CHECK(lTexture.GetTexture() == nullptr);

        // The CPU copy is released either way: holding pixels for an upload that already happened
        // (or never can) is the same waste.
        CHECK(lTexture.GetPixels().empty());
        CHECK(lTexture.GetWidth() == 4u);   // a sprite still needs its size
        CHECK(lTexture.GetResourceSize() == sizeof(Texture2D) + 24u);
    }

    TEST_CASE("is read from image files, under every spelling of their extensions")
    {
        ObjectClassRegistry lClasses;
        REQUIRE(lClasses.Register<Texture2D>());

        for (const char* lExtension : { ".png", ".PNG", "jpg", ".jpeg", ".tga", ".bmp" })
        {
            CHECK(lClasses.FindBySourceExtension(NormalizeExtension(lExtension)) == &Texture2D::StaticClass());
        }
    }

    TEST_CASE("loads through the resource manager as an asset, named after its file")
    {
        const ScopedTempDir lTemp("asset");
        lTemp.Write("Hero.png", k_Png4x2Rgb, sizeof(k_Png4x2Rgb));

        ObjectClassRegistry lClasses;
        REQUIRE(lClasses.Register<Texture2D>());

        ResourceManager lResources;
        lResources.SetObjectClasses(&lClasses);
        REQUIRE(lResources.Startup());

        const TestFolderPaths lPaths(lTemp.Path());
        {
            const TAssetHandle<Texture2D> lHandle = LoadAsset<Texture2D>(lResources, lPaths, OpaaxString("Hero.png"));

            const Texture2D* lTexture = lHandle.Get();
            REQUIRE(lTexture != nullptr);
            CHECK(lTexture->GetName() == OPAAX_ID("Hero"));
            CHECK(lTexture->GetWidth() == 4u);

            // Initialized on the main thread when it loaded: no device here, so no GPU copy and no pixels.
            CHECK(lTexture->GetPixels().empty());
        }

        lResources.Shutdown();
    }

    TEST_CASE("an image that does not load leaves no asset: the renderer draws its own placeholder")
    {
        const ScopedTempDir    lTemp("degrade");
        const TDynArray<Uint8> lBroken = BrokenPng();
        lTemp.Write("Broken.png", lBroken.data(), lBroken.size());

        ObjectClassRegistry lClasses;
        REQUIRE(lClasses.Register<Texture2D>());

        ResourceManager lResources;
        lResources.SetObjectClasses(&lClasses);
        REQUIRE(lResources.Startup());

        const TestFolderPaths lPaths(lTemp.Path());
        {
            const TAssetHandle<Texture2D> lHandle = LoadAsset<Texture2D>(lResources, lPaths, OpaaxString("Broken.png"));
            CHECK_FALSE(lHandle.IsLoaded());
            CHECK(lHandle.Get() == nullptr);
        }

        lResources.Shutdown();
    }
}
