// Suite: texture sampling — the sampler of each project filter, the mipmap levels of an image,
// and the colour given to transparent texels before upload. No device.
#include <doctest.h>

#include "Core/Image/AlphaBleed.h"
#include "RHI/TextureSampling.h"

using namespace Opaax;

namespace
{
    /** An RGBA image, every texel transparent black until set. */
    struct Image
    {
        Uint32           Width;
        Uint32           Height;
        TDynArray<Uint8> Pixels;

        Image(const Uint32 InWidth, const Uint32 InHeight)
            : Width(InWidth), Height(InHeight), Pixels(static_cast<size_t>(InWidth) * InHeight * 4, 0)
        {
        }

        void Set(const Uint32 InX, const Uint32 InY, const Uint8 InR, const Uint8 InG, const Uint8 InB, const Uint8 InA)
        {
            Uint8* lTexel = &Pixels[(static_cast<size_t>(InY) * Width + InX) * 4];
            lTexel[0] = InR; lTexel[1] = InG; lTexel[2] = InB; lTexel[3] = InA;
        }

        bool Is(const Uint32 InX, const Uint32 InY, const Uint8 InR, const Uint8 InG, const Uint8 InB, const Uint8 InA) const
        {
            const Uint8* lTexel = &Pixels[(static_cast<size_t>(InY) * Width + InX) * 4];
            return lTexel[0] == InR && lTexel[1] == InG && lTexel[2] == InB && lTexel[3] == InA;
        }

        Uint32 Bleed() { return BleedAlphaEdges(Pixels.data(), Width, Height); }
    };
}

TEST_CASE("TextureSampling: each project filter's sampler")
{
    const SamplerDesc lNearest = MakeSamplerDesc(ETextureFilter::Nearest);
    CHECK(lNearest.Reduce  == ESampleFilter::Nearest);
    CHECK(lNearest.Enlarge == ESampleFilter::Nearest);
    CHECK_FALSE(lNearest.bMipmaps);

    const SamplerDesc lPixel = MakeSamplerDesc(ETextureFilter::Pixel);
    CHECK(lPixel.Reduce  == ESampleFilter::Linear);
    CHECK(lPixel.Enlarge == ESampleFilter::Nearest);
    CHECK_FALSE(lPixel.bMipmaps);

    const SamplerDesc lSmooth = MakeSamplerDesc(ETextureFilter::Smooth);
    CHECK(lSmooth.Reduce  == ESampleFilter::Linear);
    CHECK(lSmooth.Enlarge == ESampleFilter::Linear);
    CHECK(lSmooth.bMipmaps);
}

TEST_CASE("TextureSampling: every project filter clamps, so an edge takes nothing from the opposite side")
{
    for (const ETextureFilter lFilter : TEnumValues<ETextureFilter>::Values)
    {
        CHECK(MakeSamplerDesc(lFilter).Wrap == ETextureWrap::Clamp);
    }
}

TEST_CASE("TextureSampling: a sampler nobody set up is the Pixel one")
{
    CHECK(SamplerDesc{} == MakeSamplerDesc(ETextureFilter::Pixel));
}

TEST_CASE("TextureSampling: the mipmap levels of an image")
{
    CHECK(MipLevelCount(256, 64) == 9);   // 256, 128, 64, 32, 16, 8, 4, 2, 1
    CHECK(MipLevelCount(64, 256) == 9);
    CHECK(MipLevelCount(1, 1)    == 1);
    CHECK(MipLevelCount(2, 1)    == 2);
    CHECK(MipLevelCount(100, 30) == 7);   // 100, 50, 25, 12, 6, 3, 1
    CHECK(MipLevelCount(0, 0)    == 0);

    CHECK(MipLevelSize(256, 0)  == 256);
    CHECK(MipLevelSize(256, 2)  == 64);
    CHECK(MipLevelSize(64, 2)   == 16);
    CHECK(MipLevelSize(64, 8)   == 1);    // the short side stays 1 while the long one shrinks
    CHECK(MipLevelSize(100, 3)  == 12);
    CHECK(MipLevelSize(100, 40) == 1);
}

TEST_CASE("AlphaBleed: a transparent texel beside a red one becomes red, still transparent")
{
    Image lImage(3, 1);
    lImage.Set(0, 0, 255, 0, 0, 255);

    CHECK(lImage.Bleed() == 2);
    CHECK(lImage.Is(0, 0, 255, 0, 0, 255));
    CHECK(lImage.Is(1, 0, 255, 0, 0, 0));
    CHECK(lImage.Is(2, 0, 255, 0, 0, 0));
}

TEST_CASE("AlphaBleed: an image with nothing visible does not change")
{
    Image lImage(4, 4);
    lImage.Set(1, 1, 9, 8, 7, 0);

    CHECK(lImage.Bleed() == 0);
    CHECK(lImage.Is(1, 1, 9, 8, 7, 0));
    CHECK(lImage.Is(0, 0, 0, 0, 0, 0));
}

TEST_CASE("AlphaBleed: an image with no transparent texel does not change")
{
    Image lImage(2, 2);
    lImage.Set(0, 0, 10, 20, 30, 255);
    lImage.Set(1, 0, 40, 50, 60, 255);
    lImage.Set(0, 1, 70, 80, 90, 1);
    lImage.Set(1, 1, 15, 25, 35, 128);

    CHECK(lImage.Bleed() == 0);
    CHECK(lImage.Is(0, 1, 70, 80, 90, 1));
    CHECK(lImage.Is(1, 1, 15, 25, 35, 128));
}

TEST_CASE("AlphaBleed: a visible texel keeps its colour and its alpha")
{
    Image lImage(3, 3);
    lImage.Set(1, 1, 12, 34, 56, 78);
    lImage.Bleed();

    CHECK(lImage.Is(1, 1, 12, 34, 56, 78));
}

TEST_CASE("AlphaBleed: the colour goes in every direction, corners included")
{
    Image lImage(3, 3);
    lImage.Set(1, 1, 0, 200, 0, 255);

    CHECK(lImage.Bleed() == 8);
    for (Uint32 lY = 0; lY < 3; ++lY)
    {
        for (Uint32 lX = 0; lX < 3; ++lX)
        {
            if (lX != 1 || lY != 1) { CHECK(lImage.Is(lX, lY, 0, 200, 0, 0)); }
        }
    }
}

TEST_CASE("AlphaBleed: a transparent texel takes the nearer of two colours")
{
    // Red on the left, blue on the right, seven transparent texels between.
    Image lImage(9, 1);
    lImage.Set(0, 0, 255, 0, 0, 255);
    lImage.Set(8, 0, 0, 0, 255, 255);
    lImage.Bleed();

    CHECK(lImage.Is(1, 0, 255, 0, 0, 0));
    CHECK(lImage.Is(3, 0, 255, 0, 0, 0));
    CHECK(lImage.Is(5, 0, 0, 0, 255, 0));
    CHECK(lImage.Is(7, 0, 0, 0, 255, 0));
}

TEST_CASE("AlphaBleed: the colour reaches texels far from any visible one, for the mipmap levels")
{
    Image lImage(64, 64);
    lImage.Set(0, 0, 255, 128, 0, 255);

    CHECK(lImage.Bleed() == 64 * 64 - 1);
    CHECK(lImage.Is(63, 63, 255, 128, 0, 0));
    CHECK(lImage.Is(63, 0, 255, 128, 0, 0));
    CHECK(lImage.Is(0, 63, 255, 128, 0, 0));
}

TEST_CASE("AlphaBleed: no image, no work")
{
    CHECK(BleedAlphaEdges(nullptr, 4, 4) == 0);

    Uint8 lTexel[4] = { 1, 2, 3, 0 };
    CHECK(BleedAlphaEdges(lTexel, 0, 0) == 0);
}
