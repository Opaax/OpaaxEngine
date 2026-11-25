// Suite: textures made from pixels in code (RuntimeTexture) — size and channels kept, wrong input
// refused, found by their runtime path, released once no handle holds them, a rectangle of texels
// replaced; paintable ones made premultiplied and never changed by pixels. No device here: the GPU
// side (glTexSubImage2D, painting, the drawn sprite) is checked by TestWorld/Tests/RuntimeTexture.json
// and TestWorld/Tests/PaintTexture.json.
#include <doctest.h>

#include "Core/OpaaxTypes.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/Textures/RuntimeTexture.h"
#include "Renderer/Textures/Texture2D.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/ResourceManager.h"
#include "RHI/TextureRegion.h"

using namespace Opaax;

namespace
{
    struct Fixture
    {
        ObjectClassRegistry Classes;
        ResourceManager     Resources;

        Fixture()
        {
            Classes.Register<Texture2D>();
            Resources.SetObjectClasses(&Classes);
            REQUIRE(Resources.Startup());
        }

        ~Fixture() { Resources.Shutdown(); }
    };

    /** InWidth x InHeight texels of InChannels bytes, each byte InValue. */
    TDynArray<Uint8> Texels(const Uint32 InWidth, const Uint32 InHeight, const Int32 InChannels, const Uint8 InValue)
    {
        return TDynArray<Uint8>(static_cast<size_t>(InWidth) * InHeight * InChannels, InValue);
    }
}

TEST_SUITE("TextureRegion")
{
    TEST_CASE("inside: not empty, and every texel within the texture")
    {
        CHECK(IsRegionInside(TextureRegion::Whole(4, 2), 4, 2));
        CHECK(IsRegionInside(TextureRegion{ 3, 1, 1, 1 }, 4, 2));
        CHECK_FALSE(IsRegionInside(TextureRegion{ 3, 1, 2, 1 }, 4, 2));   // one column past the edge
        CHECK_FALSE(IsRegionInside(TextureRegion{ 0, 2, 1, 1 }, 4, 2));   // one row past the top
        CHECK_FALSE(IsRegionInside(TextureRegion{ 0, 0, 0, 1 }, 4, 2));   // empty
        CHECK_FALSE(IsRegionInside(TextureRegion{ 0xFFFFFFFFu, 0, 2, 1 }, 4, 2));   // X + Width wraps
    }

    TEST_CASE("bytes: width x height x channels")
    {
        CHECK(RegionByteCount(TextureRegion{ 1, 1, 3, 2 }, 4) == 24u);
        CHECK(RegionByteCount(TextureRegion{ 1, 1, 3, 2 }, 1) == 6u);
    }
}

TEST_SUITE("RuntimeTexture")
{
    TEST_CASE("made from pixels: its size and channels, found by its runtime path")
    {
        Fixture lFixture;

        const TDynArray<Uint8> lPixels = Texels(8, 4, 1, 0);
        RuntimeTexture         lFog    = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Fog"),
                                                                RuntimeTextureDesc{ 8, 4, 1 }, lPixels.data());
        REQUIRE(lFog.IsValid());
        CHECK(lFog.GetPath() == OpaaxString("/Runtime/Fog"));
        CHECK(lFog.GetWidth() == 8u);
        CHECK(lFog.GetHeight() == 4u);
        CHECK(lFog.GetChannels() == 1);
        CHECK(lFog.GetTexture()->IsWritable());

        // A sprite names it like an image file.
        const TAssetHandle<Texture2D> lNamed =
            LoadAsset<Texture2D>(lFixture.Resources, IPaths::Null(), OpaaxString("/Runtime/Fog"));
        CHECK(lNamed.Get() == lFog.GetTexture());
    }

    TEST_CASE("released at the next resource update once no handle holds it")
    {
        Fixture lFixture;

        const TDynArray<Uint8> lPixels = Texels(2, 2, 4, 255);
        RuntimeTexture         lFirst  = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Map"),
                                                                RuntimeTextureDesc{ 2, 2 }, lPixels.data());
        RuntimeTexture lCopy = lFirst;   // copies share it
        REQUIRE(lCopy.IsValid());

        lFirst.Reset();
        lFixture.Resources.Update(0.0);
        CHECK(lCopy.IsValid());
        CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 1u);

        lCopy.Reset();
        lFixture.Resources.Update(0.0);
        CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 0u);
        CHECK_FALSE(LoadAsset<Texture2D>(lFixture.Resources, IPaths::Null(), OpaaxString("/Runtime/Map")).IsLoaded());
    }

    TEST_CASE("refused: a path outside /Runtime/, a path in use, 2 or 3 channels, no size, no pixels")
    {
        Fixture lFixture;

        const TDynArray<Uint8> lPixels = Texels(2, 2, 4, 0);
        const RuntimeTexture   lKept   = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/A"),
                                                                RuntimeTextureDesc{ 2, 2 }, lPixels.data());
        REQUIRE(lKept.IsValid());

        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("Textures/A.png"),
                                           RuntimeTextureDesc{ 2, 2 }, lPixels.data()).IsValid());
        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/A"),
                                           RuntimeTextureDesc{ 2, 2 }, lPixels.data()).IsValid());
        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/B"),
                                           RuntimeTextureDesc{ 2, 2, 3 }, lPixels.data()).IsValid());
        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/B"),
                                           RuntimeTextureDesc{ 2, 2, 2 }, lPixels.data()).IsValid());
        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/B"),
                                           RuntimeTextureDesc{ 0, 2 }, lPixels.data()).IsValid());
        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/B"),
                                           RuntimeTextureDesc{ 2, 2 }, nullptr).IsValid());

        CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 1u);
    }

    TEST_CASE("without a device nothing is uploaded, so an update is refused")
    {
        Fixture lFixture;

        const TDynArray<Uint8> lPixels = Texels(2, 2, 4, 0);
        RuntimeTexture         lMap    = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Map"),
                                                                RuntimeTextureDesc{ 2, 2 }, lPixels.data());
        REQUIRE(lMap.IsValid());
        CHECK(lMap.GetTexture()->GetTexture() == nullptr);   // IEngine::Null(): no device
        CHECK_FALSE(lMap.Update(lPixels.data()));
        CHECK_FALSE(RuntimeTexture{}.Update(lPixels.data()));
    }

    TEST_CASE("before the upload, a rectangle changes those texels only")
    {
        // A texture not yet uploaded keeps its pixels: the copy into them is the region logic.
        Texture2D lTexture;
        const TDynArray<Uint8> lBlack = Texels(4, 3, 1, 0);
        REQUIRE(lTexture.InitFromPixels(4, 3, 1, lBlack.data(), false));

        const TDynArray<Uint8> lWhite = Texels(2, 2, 1, 255);
        REQUIRE(lTexture.UpdatePixels(TextureRegion{ 1, 1, 2, 2 }, lWhite.data()));

        // Rows bottom first: row 0 untouched, rows 1 and 2 white in columns 1 and 2.
        const TDynArray<Uint8> lExpected = { 0, 0,   0,   0,
                                             0, 255, 255, 0,
                                             0, 255, 255, 0 };
        CHECK(lTexture.GetPixels() == lExpected);

        CHECK_FALSE(lTexture.UpdatePixels(TextureRegion{ 3, 0, 2, 1 }, lWhite.data()));   // past the edge
        CHECK(lTexture.GetPixels() == lExpected);
    }

    TEST_CASE("a texture read from a file never changes")
    {
        Texture2D              lTexture;
        const TDynArray<Uint8> lPixels = Texels(1, 1, 4, 0);
        CHECK_FALSE(lTexture.IsWritable());
        CHECK_FALSE(lTexture.UpdatePixels(TextureRegion::Whole(1, 1), lPixels.data()));
    }

    TEST_CASE("paintable: made with or without pixels, 4 channels only")
    {
        Fixture lFixture;

        RuntimeTextureDesc lDesc{ 2, 2 };
        lDesc.bPaintable = true;

        const RuntimeTexture lEmpty = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Empty"), lDesc, nullptr);
        REQUIRE(lEmpty.IsValid());
        CHECK(lEmpty.IsPaintable());
        CHECK(lEmpty.GetChannels() == 4);

        const TDynArray<Uint8> lPixels = Texels(2, 2, 4, 255);
        const RuntimeTexture lFilled = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Filled"), lDesc,
                                                              lPixels.data());
        CHECK(lFilled.IsPaintable());

        RuntimeTextureDesc lMask{ 2, 2, 1 };
        lMask.bPaintable = true;
        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Mask"), lMask, nullptr).IsValid());

        RuntimeTextureDesc lFlat{ 0, 2 };
        lFlat.bPaintable = true;
        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Flat"), lFlat, nullptr).IsValid());

        CHECK_FALSE(RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Plain"), RuntimeTextureDesc{ 2, 2 },
                                           lPixels.data()).IsPaintable());
    }

    TEST_CASE("paintable: its pixels wait premultiplied, transparent when none were given")
    {
        // Straight red at half alpha, then opaque white: rows bottom first.
        const TDynArray<Uint8> lStraight = { 255, 0, 0, 128,   255, 255, 255, 255 };

        Texture2D lGiven;
        REQUIRE(lGiven.InitPaintable(2, 1, lStraight.data()));
        CHECK(lGiven.IsPaintable());
        const TDynArray<Uint8> lPremultiplied = { 128, 0, 0, 128,   255, 255, 255, 255 };
        CHECK(lGiven.GetPixels() == lPremultiplied);

        Texture2D lNone;
        REQUIRE(lNone.InitPaintable(2, 1, nullptr));
        CHECK(lNone.GetPixels() == Texels(2, 1, 4, 0));

        CHECK_FALSE(lNone.InitPaintable(2, 1, nullptr));   // only once
    }

    TEST_CASE("paintable: its pixels are never replaced")
    {
        Fixture lFixture;

        Texture2D lTexture;
        REQUIRE(lTexture.InitPaintable(2, 2, nullptr));
        const TDynArray<Uint8> lWhite = Texels(1, 1, 4, 255);
        CHECK_FALSE(lTexture.UpdatePixels(TextureRegion{ 0, 0, 1, 1 }, lWhite.data()));
        CHECK(lTexture.GetPixels() == Texels(2, 2, 4, 0));

        RuntimeTextureDesc lDesc{ 2, 2 };
        lDesc.bPaintable = true;
        RuntimeTexture lPaint = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Paint"), lDesc, nullptr);
        REQUIRE(lPaint.IsValid());
        CHECK_FALSE(lPaint.UpdateRegion(TextureRegion{ 0, 0, 1, 1 }, lWhite.data()));
        CHECK(lPaint.GetTexture()->GetPaintTarget() == nullptr);   // IEngine::Null(): no device
    }
}
