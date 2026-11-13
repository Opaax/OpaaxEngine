// Suite: TRenderTargetCache — the targets views are drawn through, kept from frame to frame:
// one per size in use, an idle one resized for a new size, an unused one dropped.
// Tested with a plain struct as the target: no device.
#include <doctest.h>

#include "Renderer/Post/RenderTargetCache.h"

using namespace Opaax;

namespace
{
    struct FakeTarget
    {
        Uint32 Width  = 0;
        Uint32 Height = 0;
        Uint32 Id     = 0;
    };

    /** A cache and the functions it is given, with what they were asked. */
    struct Fixture
    {
        TRenderTargetCache<FakeTarget> Cache;
        Uint32                         NextId = 1;

        FakeTarget& Acquire(const RenderTargetKey& InKey)
        {
            return Cache.Acquire(InKey,
                [this](const RenderTargetKey& InNew)
                {
                    return MakeUnique<FakeTarget>(FakeTarget{ InNew.Width, InNew.Height, NextId++ });
                },
                [](FakeTarget& InTarget, const RenderTargetKey& InNew)
                {
                    InTarget.Width  = InNew.Width;
                    InTarget.Height = InNew.Height;
                });
        }
    };

    constexpr Uint32 SCENE = 0;
    constexpr Uint32 BLOOM = 1;

    RenderTargetKey Key(const Uint32 InPurpose, const Uint32 InWidth, const Uint32 InHeight,
                        const ETextureFormat InFormat = ETextureFormat::RGBA16F)
    {
        RenderTargetKey lKey;
        lKey.Purpose = InPurpose;
        lKey.Width   = InWidth;
        lKey.Height  = InHeight;
        lKey.Format  = InFormat;
        return lKey;
    }
}

TEST_CASE("RenderTargetCache: the same key twice gives one target")
{
    Fixture lFixture;

    FakeTarget& lFirst  = lFixture.Acquire(Key(SCENE, 1280, 720));
    FakeTarget& lSecond = lFixture.Acquire(Key(SCENE, 1280, 720));

    CHECK(&lFirst == &lSecond);
    CHECK(lFixture.Cache.GetCount() == 1);
    CHECK(lFixture.Cache.GetCreated() == 1);
}

TEST_CASE("RenderTargetCache: another sample count is another target, never resized into it")
{
    Fixture lFixture;

    RenderTargetKey lMultisampled = Key(SCENE, 1280, 720);
    lMultisampled.Samples = 4;

    FakeTarget& lOne  = lFixture.Acquire(Key(SCENE, 1280, 720));
    FakeTarget& lFour = lFixture.Acquire(lMultisampled);
    CHECK(&lOne != &lFour);

    // Idle for a while, the one-sample target is still not taken for a multisampled size.
    for (Uint32 lFrame = 0; lFrame < 5; ++lFrame)
    {
        lFixture.Cache.EndFrame();
        lFixture.Acquire(lMultisampled);
    }

    lMultisampled.Width = 640;
    lFixture.Acquire(lMultisampled);
    CHECK(lFixture.Cache.GetResized() == 0);
    CHECK(lFixture.Cache.GetCreated() == 3);
}

TEST_CASE("RenderTargetCache: two sizes in a frame get a target each, and none is remade the next frames")
{
    Fixture lFixture;

    for (Uint32 lFrame = 0; lFrame < 10; ++lFrame)
    {
        FakeTarget& lGame    = lFixture.Acquire(Key(SCENE, 660, 207));
        FakeTarget& lPreview = lFixture.Acquire(Key(SCENE, 320, 180));

        CHECK(&lGame != &lPreview);
        CHECK(lGame.Width == 660);
        CHECK(lPreview.Width == 320);

        lFixture.Cache.EndFrame();
    }

    CHECK(lFixture.Cache.GetCount() == 2);
    CHECK(lFixture.Cache.GetCreated() == 2);
    CHECK(lFixture.Cache.GetResized() == 0);
}

TEST_CASE("RenderTargetCache: two purposes of one size do not share a target")
{
    Fixture lFixture;

    FakeTarget& lScene = lFixture.Acquire(Key(SCENE, 640, 360));
    FakeTarget& lBloom = lFixture.Acquire(Key(BLOOM, 640, 360));

    CHECK(&lScene != &lBloom);
    CHECK(lFixture.Cache.GetCount() == 2);
}

TEST_CASE("RenderTargetCache: another format or filter is another target")
{
    Fixture lFixture;

    FakeTarget& lHdr = lFixture.Acquire(Key(SCENE, 640, 360, ETextureFormat::RGBA16F));
    FakeTarget& lLdr = lFixture.Acquire(Key(SCENE, 640, 360, ETextureFormat::RGBA8));

    RenderTargetKey lNearest = Key(SCENE, 640, 360, ETextureFormat::RGBA8);
    lNearest.bLinearFilter   = false;
    FakeTarget& lUnfiltered  = lFixture.Acquire(lNearest);

    CHECK(&lHdr != &lLdr);
    CHECK(&lLdr != &lUnfiltered);
    CHECK(lFixture.Cache.GetCount() == 3);
}

TEST_CASE("RenderTargetCache: a view being resized reuses its old sizes instead of piling targets up")
{
    Fixture lFixture;

    // A new size every frame, as when a panel's edge is dragged.
    for (Uint32 lFrame = 0; lFrame < 50; ++lFrame)
    {
        FakeTarget& lTarget = lFixture.Acquire(Key(SCENE, 600 + lFrame, 400));
        CHECK(lTarget.Width == 600 + lFrame);
        lFixture.Cache.EndFrame();
    }

    // Two targets take turns: the one of the frame before may still be in use.
    CHECK(lFixture.Cache.GetCount() == 2);
    CHECK(lFixture.Cache.GetCreated() == 2);
    CHECK(lFixture.Cache.GetResized() == 48);
}

TEST_CASE("RenderTargetCache: a target used the frame before is not taken over by a new size")
{
    Fixture lFixture;

    FakeTarget& lGame = lFixture.Acquire(Key(SCENE, 660, 207));
    lFixture.Cache.EndFrame();

    // The game view was not drawn yet this frame when another size asks.
    FakeTarget& lPreview = lFixture.Acquire(Key(SCENE, 320, 180));

    CHECK(&lGame != &lPreview);
    CHECK(lGame.Width == 660);
}

TEST_CASE("RenderTargetCache: a target of another kind is never resized for a key")
{
    Fixture lFixture;

    lFixture.Acquire(Key(BLOOM, 330, 103));
    for (Uint32 lFrame = 0; lFrame < 5; ++lFrame)
    {
        lFixture.Cache.EndFrame();
    }

    // Idle long enough to be taken over, but it is a bloom level.
    lFixture.Acquire(Key(SCENE, 660, 207));

    CHECK(lFixture.Cache.GetCount() == 2);
    CHECK(lFixture.Cache.GetResized() == 0);
}

TEST_CASE("RenderTargetCache: a target unused for a while is dropped, one in use is kept")
{
    Fixture lFixture;
    lFixture.Acquire(Key(SCENE, 320, 180));

    for (Uint32 lFrame = 0; lFrame <= TRenderTargetCache<FakeTarget>::MAX_IDLE_FRAMES; ++lFrame)
    {
        lFixture.Acquire(Key(SCENE, 660, 207));
        lFixture.Cache.EndFrame();
    }

    CHECK(lFixture.Cache.GetCount() == 1);
    CHECK(lFixture.Cache.GetDropped() == 1);
    CHECK(lFixture.Acquire(Key(SCENE, 660, 207)).Width == 660);
    CHECK(lFixture.Cache.GetCreated() == 2);
}

TEST_CASE("RenderTargetCache: dropping one target keeps the others right")
{
    Fixture lFixture;

    // Three made in a frame; only the middle one stops being used.
    for (Uint32 lFrame = 0; lFrame <= TRenderTargetCache<FakeTarget>::MAX_IDLE_FRAMES + 1; ++lFrame)
    {
        lFixture.Acquire(Key(SCENE, 100, 100));
        if (lFrame == 0)
        {
            lFixture.Acquire(Key(BLOOM, 200, 200));
        }
        lFixture.Acquire(Key(SCENE, 300, 300));
        lFixture.Cache.EndFrame();
    }

    CHECK(lFixture.Cache.GetCount() == 2);
    CHECK(lFixture.Acquire(Key(SCENE, 100, 100)).Width == 100);
    CHECK(lFixture.Acquire(Key(SCENE, 300, 300)).Width == 300);
    CHECK(lFixture.Cache.GetCreated() == 3);
}

TEST_CASE("RenderTargetCache: Clear drops everything")
{
    Fixture lFixture;
    lFixture.Acquire(Key(SCENE, 100, 100));
    lFixture.Acquire(Key(BLOOM, 50, 50));

    lFixture.Cache.Clear();

    CHECK(lFixture.Cache.GetCount() == 0);
}
