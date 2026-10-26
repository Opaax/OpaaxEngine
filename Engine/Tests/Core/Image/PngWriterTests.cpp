// Suite: PngWriter — what it writes is read back by the engine's own image decoder (stb, through
// Texture2D::LoadSource), which checks every chunk CRC and the zlib checksum. Texture2D flips rows for
// OpenGL, so its first row is the image's bottom row.
#include <doctest.h>

#include "../../TestTempDir.h"
#include "Core/Image/PngWriter.h"
#include "Renderer/Textures/Texture2D.h"

using namespace Opaax;

namespace
{
    class TempFolder
    {
    public:
        explicit TempFolder(const char* InTag) : m_Dir(InTag) {}

        OpaaxString File(const char* InName) const { return m_Dir.Sub(InName); }

    private:
        TestTempDir m_Dir;
    };

    /** Decodes a PNG file the way the engine does, without uploading it. */
    bool Decode(const OpaaxString& InPath, Texture2D& OutTexture)
    {
        return OutTexture.LoadSource(InPath.CStr());
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
    void CheckRoundTrip(const Texture2D& InDecoded, const Uint32 InWidth, const Uint32 InHeight, const Uint32 InChannels)
    {
        REQUIRE(InDecoded.GetWidth() == InWidth);
        REQUIRE(InDecoded.GetHeight() == InHeight);
        REQUIRE(InDecoded.GetChannels() == static_cast<Int32>(InChannels));
        REQUIRE(InDecoded.GetPixels().size() == static_cast<size_t>(InWidth) * InHeight * InChannels);

        Uint32 lMismatches = 0;
        for (Uint32 lY = 0; lY < InHeight; ++lY)
        {
            const Uint32 lDecodedRow = InHeight - 1 - lY;
            for (Uint32 lX = 0; lX < InWidth; ++lX)
            {
                for (Uint32 lC = 0; lC < InChannels; ++lC)
                {
                    const size_t lAt = (static_cast<size_t>(lDecodedRow) * InWidth + lX) * InChannels + lC;
                    if (InDecoded.GetPixels()[lAt] != Channel(lX, lY, lC)) { ++lMismatches; }
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

    Texture2D lTexture;
    REQUIRE(Decode(lPath, lTexture));
    CheckRoundTrip(lTexture, 5, 3, 4);
}

TEST_CASE("PngWriter: an RGB image larger than one stored block reads back exactly")
{
    // 200 x 200 x 3 = 120 000 bytes of image data: more than one 65 535-byte stored block.
    const TempFolder lFolder("rgb");
    const OpaaxString lPath = lFolder.File("Big.png");

    const TDynArray<Uint8> lImage = MakeImage(200, 200, 3);
    REQUIRE(PngWriter::Write(lPath, 200, 200, 3, lImage.data()));

    Texture2D lTexture;
    REQUIRE(Decode(lPath, lTexture));
    CheckRoundTrip(lTexture, 200, 200, 3);
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

    const OpaaxStringView lEnd(reinterpret_cast<const char*>(lPng.data()) + lPng.size() - 8, 4);
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
