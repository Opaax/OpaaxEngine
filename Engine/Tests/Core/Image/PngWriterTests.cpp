// Suite: PngWriter — what it writes is read back by the engine's own image decoder (stb, through
// TextureResource::Load), which checks every chunk CRC and the zlib checksum. TextureResource flips
// rows for OpenGL, so its first row is the image's bottom row.
#include <doctest.h>

#include <filesystem>
#include <string>

#include "Core/Image/PngWriter.h"
#include "Renderer/Textures/TextureResource.h"
#include "Resources/ResourceDependencyGraph.hpp"
#include "Resources/ResourceManager.h"   // completes LoadContext

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    class TempFolder
    {
    public:
        explicit TempFolder(const char* InTag)
            : m_Path(fs::temp_directory_path() / ("OpaaxPngWriterTests_" + std::string(InTag)))
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ~TempFolder()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        TempFolder(const TempFolder&)            = delete;
        TempFolder& operator=(const TempFolder&) = delete;

        OpaaxString File(const char* InName) const
        {
            return OpaaxString((m_Path / InName).generic_string().c_str());
        }

    private:
        fs::path m_Path;
    };

    /** Decodes a PNG file the way the engine does, without uploading it. */
    std::optional<TextureResource> Decode(const OpaaxString& InPath)
    {
        ResourceManager         lManager;
        ResourceDependencyGraph lDeps;
        LoadContext             lContext{ lManager, lDeps };
        return TextureResource::Load(InPath.CStr(), lContext);
    }

    /** A pixel's colour from its coordinates, different everywhere. */
    Uint8 Channel(const Uint32 InX, const Uint32 InY, const Uint32 InChannel)
    {
        return static_cast<Uint8>((InX * 37 + InY * 91 + InChannel * 53) & 0xFF);
    }

    TDynArray<Uint8> MakeImage(const Uint32 InWidth, const Uint32 InHeight, const Uint32 InChannels)
    {
        TDynArray<Uint8> lPixels;
        for (Uint32 lY = 0; lY < InHeight; ++lY)
        {
            for (Uint32 lX = 0; lX < InWidth; ++lX)
            {
                for (Uint32 lC = 0; lC < InChannels; ++lC) { lPixels.push_back(Channel(lX, lY, lC)); }
            }
        }
        return lPixels;
    }

    /** Checks that the decoded (bottom-up) pixels match the written (top-down) image. */
    void CheckRoundTrip(const TextureResource& InDecoded, const Uint32 InWidth, const Uint32 InHeight, const Uint32 InChannels)
    {
        REQUIRE(InDecoded.Width == InWidth);
        REQUIRE(InDecoded.Height == InHeight);
        REQUIRE(InDecoded.Channels == static_cast<Int32>(InChannels));
        REQUIRE(InDecoded.Pixels.size() == static_cast<size_t>(InWidth) * InHeight * InChannels);

        Uint32 lMismatches = 0;
        for (Uint32 lY = 0; lY < InHeight; ++lY)
        {
            const Uint32 lDecodedRow = InHeight - 1 - lY;
            for (Uint32 lX = 0; lX < InWidth; ++lX)
            {
                for (Uint32 lC = 0; lC < InChannels; ++lC)
                {
                    const size_t lAt = (static_cast<size_t>(lDecodedRow) * InWidth + lX) * InChannels + lC;
                    if (InDecoded.Pixels[lAt] != Channel(lX, lY, lC)) { ++lMismatches; }
                }
            }
        }
        CHECK(lMismatches == 0);
    }
}

TEST_CASE("PngWriter: an RGBA image reads back exactly")
{
    const TempFolder lFolder("rgba");
    const OpaaxString lPath = lFolder.File("Capture.png");

    const TDynArray<Uint8> lImage = MakeImage(5, 3, 4);
    REQUIRE(PngWriter::Write(lPath, 5, 3, 4, lImage.data()));

    const std::optional<TextureResource> lTexture = Decode(lPath);
    REQUIRE(lTexture.has_value());
    CheckRoundTrip(*lTexture, 5, 3, 4);
}

TEST_CASE("PngWriter: an RGB image larger than one stored block reads back exactly")
{
    // 200 x 200 x 3 = 120 000 bytes of image data: more than one 65 535-byte stored block.
    const TempFolder lFolder("rgb");
    const OpaaxString lPath = lFolder.File("Big.png");

    const TDynArray<Uint8> lImage = MakeImage(200, 200, 3);
    REQUIRE(PngWriter::Write(lPath, 200, 200, 3, lImage.data()));

    const std::optional<TextureResource> lTexture = Decode(lPath);
    REQUIRE(lTexture.has_value());
    CheckRoundTrip(*lTexture, 200, 200, 3);
}

TEST_CASE("PngWriter: the file starts with the PNG signature and ends with IEND")
{
    const TDynArray<Uint8> lImage = MakeImage(2, 2, 4);
    const TDynArray<Uint8> lPng   = PngWriter::Encode(2, 2, 4, lImage.data());

    REQUIRE(lPng.size() > 8 + 12);
    CHECK(lPng[0] == 0x89);
    CHECK(lPng[1] == 'P');
    CHECK(lPng[2] == 'N');
    CHECK(lPng[3] == 'G');

    const std::string lEnd(lPng.end() - 8, lPng.end() - 4);
    CHECK(lEnd == "IEND");
}

TEST_CASE("PngWriter: an invalid image gives nothing")
{
    const TDynArray<Uint8> lImage = MakeImage(2, 2, 4);

    CHECK(PngWriter::Encode(0, 2, 4, lImage.data()).empty());
    CHECK(PngWriter::Encode(2, 0, 4, lImage.data()).empty());
    CHECK(PngWriter::Encode(2, 2, 2, lImage.data()).empty());
    CHECK(PngWriter::Encode(2, 2, 4, nullptr).empty());
    CHECK_FALSE(PngWriter::Write(OpaaxString(), 2, 2, 4, lImage.data()));
}
