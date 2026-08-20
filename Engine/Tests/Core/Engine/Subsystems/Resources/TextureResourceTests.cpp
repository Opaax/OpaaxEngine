// Suite: TextureResource — the first GPU-BACKED resource, and the first production caller of the
// many-extensions-to-one-type format table.
//
// What is testable headless is the half that matters most: Load is pure file IO + CPU decode, so it
// runs here exactly as it runs on a worker. The GPU half cannot be — there is no locator and no
// device in this exe — and that is a designed property, not a hole: IEngine::Null() answers nullptr,
// so Initialize degrades to "decoded, not uploaded" instead of crashing (I3). The upload itself is
// gated by the runtime smoke test, which is the only place a GL context exists.
//
// The fixture PNG is 4x2 with a RED top row and a BLUE bottom row. Both asymmetries are deliberate:
// 4 != 2 catches a swapped width/height, and the row colours catch a missing vertical flip — which
// no dimension check could see, and which would silently render every sprite upside down.
//
// Runs against a unique temp directory, created and removed per case — never the repo's assets ([[L20]]).
#include <doctest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "Engine/Subsystems/Resources/ResourceManager.h"        // completes LoadContext
#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"
#include "Engine/Subsystems/Resources/Types/TextureResource.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    // 4x2 RGB PNG: top row red, bottom row blue. Written by a one-shot generator, kept as bytes so
    // the suite depends on no image file and no content the user might move.
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
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxTextureTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);          // a previous crashed run must not poison this one
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        OpaaxString Sub(const char* InRel) const
        {
            return OpaaxString((m_Path / InRel).generic_string().c_str());
        }

        /** Write raw bytes to InRel and answer its absolute path. */
        OpaaxString Write(const char* InRel, const Uint8* InBytes, Uint64 InCount) const
        {
            std::ofstream lFile(m_Path / InRel, std::ios::binary);
            lFile.write(reinterpret_cast<const char*>(InBytes), static_cast<std::streamsize>(InCount));
            lFile.close();

            return Sub(InRel);
        }

    private:
        fs::path m_Path;
    };

    /** A context to hand Load. Every case is a leaf load — nothing here acquires a child. */
    struct LoadFixture
    {
        ResourceManager Manager;
        ResourceDependencyGraph Deps;
        LoadContext Ctx{ Manager, Deps };
    };
}

TEST_SUITE("TextureResource")
{
    TEST_CASE("decodes dimensions and channels from a real PNG")
    {
        const ScopedTempDir lTemp("decode");
        const OpaaxString   lPath = lTemp.Write("Sprite.png", k_Png4x2Rgb, sizeof(k_Png4x2Rgb));

        LoadFixture lFixture;
        std::optional<TextureResource> lTexture = TextureResource::Load(lPath.CStr(), lFixture.Ctx);

        REQUIRE(lTexture.has_value());
        CHECK(lTexture->Width    == 4u);
        CHECK(lTexture->Height   == 2u);
        CHECK(lTexture->Channels == 3);
        CHECK(lTexture->Pixels.size() == 4u * 2u * 3u);

        // Reported from the DIMENSIONS, so it answers the same before and after the upload frees
        // the CPU copy — the property the pool's accounting rests on.
        CHECK(lTexture->ByteSize() == sizeof(TextureResource) + 24u);
    }

    TEST_CASE("decoded rows are FLIPPED for GL's bottom-up sampling")
    {
        const ScopedTempDir lTemp("flip");
        const OpaaxString   lPath = lTemp.Write("Sprite.png", k_Png4x2Rgb, sizeof(k_Png4x2Rgb));

        LoadFixture lFixture;
        std::optional<TextureResource> lTexture = TextureResource::Load(lPath.CStr(), lFixture.Ctx);

        REQUIRE(lTexture.has_value());
        REQUIRE(lTexture->Pixels.size() >= 3u);

        // The file's FIRST row is red; after the flip the buffer's first row must be the file's
        // LAST row, which is blue. Without the flip every sprite draws upside down and nothing
        // else in the engine would ever report it.
        CHECK(lTexture->Pixels[0] == 0u);
        CHECK(lTexture->Pixels[1] == 0u);
        CHECK(lTexture->Pixels[2] == 255u);
    }

    TEST_CASE("a file that is not an image is refused, not half-loaded")
    {
        const ScopedTempDir lTemp("corrupt");

        // A PNG signature followed by garbage — it passes any "is this a png?" sniff and fails to
        // decode, which is the case a bare extension check would wave through.
        Uint8 lBroken[sizeof(k_Png4x2Rgb)];
        std::memcpy(lBroken, k_Png4x2Rgb, sizeof(k_Png4x2Rgb));
        lBroken[30] = 0x00;
        lBroken[45] = 0x7F;

        const OpaaxString lPath = lTemp.Write("Broken.png", lBroken, sizeof(lBroken));

        LoadFixture lFixture;
        CHECK_FALSE(TextureResource::Load(lPath.CStr(), lFixture.Ctx).has_value());
    }

    TEST_CASE("a missing file is refused")
    {
        const ScopedTempDir lTemp("missing");

        LoadFixture lFixture;
        CHECK_FALSE(TextureResource::Load(lTemp.Sub("Nope.png").CStr(), lFixture.Ctx).has_value());
    }

    TEST_CASE("the placeholder is a real magenta image")
    {
        const TextureResource lPlaceholder = TextureResource::Placeholder();

        CHECK(lPlaceholder.Width  == 2u);
        CHECK(lPlaceholder.Height == 2u);
        CHECK(lPlaceholder.Channels == 4);
        CHECK(lPlaceholder.Pixels.size() == 2u * 2u * 4u);

        // Magenta, opaque — loud on purpose. A placeholder that decoded to transparent black
        // would be indistinguishable from "nothing was drawn".
        CHECK(lPlaceholder.Pixels[0] == 255u);
        CHECK(lPlaceholder.Pixels[1] == 0u);
        CHECK(lPlaceholder.Pixels[2] == 255u);
        CHECK(lPlaceholder.Pixels[3] == 255u);
    }

    TEST_CASE("Initialize with no device degrades instead of crashing")
    {
        const ScopedTempDir lTemp("no_device");
        const OpaaxString   lPath = lTemp.Write("Sprite.png", k_Png4x2Rgb, sizeof(k_Png4x2Rgb));

        LoadFixture lFixture;
        std::optional<TextureResource> lTexture = TextureResource::Load(lPath.CStr(), lFixture.Ctx);
        REQUIRE(lTexture.has_value());

        lTexture->Initialize();   // IEngine::Null() -> no device (I3)

        CHECK_FALSE(lTexture->IsUploaded());
        CHECK(lTexture->GetTexture() == nullptr);

        // The CPU copy is released either way: Initialize hands ownership to the GPU, and holding
        // pixels for an upload that already happened (or never can) is the same waste.
        CHECK(lTexture->Pixels.empty());
        CHECK(lTexture->Width == 4u);   // dimensions SURVIVE — a sprite still needs its size
    }

    TEST_CASE("a broken texture RESOLVES to the magenta placeholder, never to null")
    {
        // The Placeholder policy's whole claim, through the real manager: a renderable that fails
        // to load degrades to a visible substitute, so a sprite still draws something and the frame
        // stays correct. FailFast types (Level, Map) answer null instead — that is the difference.
        const ScopedTempDir lTemp("degrade");

        Uint8 lBroken[sizeof(k_Png4x2Rgb)];
        std::memcpy(lBroken, k_Png4x2Rgb, sizeof(k_Png4x2Rgb));
        lBroken[30] = 0x00;
        lBroken[45] = 0x7F;

        const OpaaxString lPath = lTemp.Write("Broken.png", lBroken, sizeof(lBroken));

        ResourceManager              lResources;
        ResourceRef<TextureResource> lRef = lResources.Load<TextureResource>(lPath.CStr());

        CHECK_FALSE(lRef.IsValid());   // the load itself failed, and says so

        TextureResource* lResolved = lRef.Get();
        REQUIRE(lResolved != nullptr);
        CHECK(lResolved->Width  == 2u);
        CHECK(lResolved->Height == 2u);

        lResources.FlushAll();
    }

    TEST_CASE("ONE type claims every image extension it decodes")
    {
        // The multi-extension form's first production caller: before this type existed, only a test
        // probe claimed more than one spelling. Both must resolve to the SAME id, or the browser
        // would need one entry per extension and the editor would learn what a .jpg is.
        ResourceFormatRegistry lRegistry;
        REQUIRE(lRegistry.Register<TextureResource>(OPAAX_ID("Texture")));

        const ResourceFormatEntry* lPng = lRegistry.FindByExtension(NormalizeExtension(".png"));
        const ResourceFormatEntry* lJpg = lRegistry.FindByExtension(NormalizeExtension(".JPG"));
        const ResourceFormatEntry* lTga = lRegistry.FindByExtension(NormalizeExtension("tga"));

        REQUIRE(lPng != nullptr);
        REQUIRE(lJpg != nullptr);
        REQUIRE(lTga != nullptr);

        CHECK(lPng->TypeId == ResourceTypeID::Get<TextureResource>());
        CHECK(lJpg->TypeId == lPng->TypeId);
        CHECK(lTga->TypeId == lPng->TypeId);

        // One TYPE, not one per spelling.
        CHECK(lRegistry.Count() == 1u);
    }
}
