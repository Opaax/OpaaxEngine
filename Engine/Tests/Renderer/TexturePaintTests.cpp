// Suite: painting into a texture, the CPU side — stamps and clears queued per texture in call order,
// what is refused or dropped, the clear rectangle in texels, the stamps' order in a pass and the
// texel view a pass draws with. No device: the drawing itself is checked by
// TestWorld/Tests/PaintTexture.json.
#include <doctest.h>

#include "Core/OpaaxTypes.h"
#include "Core/Templates/TypeTraits.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Textures/RuntimeTexture.h"
#include "Renderer/Textures/Texture2D.h"
#include "Renderer/Textures/TexturePaint.h"
#include "Resources/ResourceManager.h"
#include "RHI/Texture.h"
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

        /** A paintable InWidth x InHeight texture under InPath, transparent. */
        RuntimeTexture MakePaintable(const char* InPath, const Uint32 InWidth = 64, const Uint32 InHeight = 64)
        {
            RuntimeTextureDesc lDesc{ InWidth, InHeight };
            lDesc.bPaintable = true;
            RuntimeTexture lTexture = RuntimeTexture::Create(Resources, OpaaxString(InPath), lDesc, nullptr);
            REQUIRE(lTexture.IsValid());
            return lTexture;
        }
    };

    /** A texture that only says whether it is paintable. */
    class FakeTexture final : public ITexture2D
    {
    public:
        explicit FakeTexture(const bool bInPaintable) : m_bPaintable(bInPaintable) {}

        void   Bind(Uint32) const override {}
        void   Unbind() const override {}
        Uint32 GetWidth()      const noexcept override { return 1; }
        Uint32 GetHeight()     const noexcept override { return 1; }
        Uint32 GetRendererID() const noexcept override { return 0; }
        bool   IsLoaded()      const noexcept override { return true; }
        bool   IsPaintable()   const noexcept override { return m_bPaintable; }

    private:
        bool m_bPaintable = false;
    };

    PaintStamp Stamp(const float InX, const float InY, const float InSize = 4.f)
    {
        PaintStamp lStamp;
        lStamp.Center = { InX, InY };
        lStamp.Size   = { InSize, InSize };
        return lStamp;
    }
}

TEST_SUITE("TexturePaint")
{
    TEST_CASE("two paint calls on one texture: one entry, one run of stamps in call order")
    {
        Fixture        lFixture;
        RuntimeTexture lTile = lFixture.MakePaintable("/Runtime/Tile");

        PaintQueue lQueue;
        CHECK(lQueue.IsEmpty());
        CHECK(lQueue.AddStamps(lTile, { Stamp(1.f, 1.f), Stamp(2.f, 2.f) }, EPaintMode::Over) == 0u);

        // Another handle on the same texture adds to the same entry.
        const RuntimeTexture lCopy = lTile;
        CHECK(lQueue.AddStamps(lCopy, { Stamp(3.f, 3.f) }, EPaintMode::Over) == 0u);

        REQUIRE(lQueue.GetWork().size() == 1u);
        const PaintWork& lWork = lQueue.GetWork()[0];
        REQUIRE(lWork.Ops.size() == 1u);
        CHECK(lWork.Ops[0].Kind == EPaintOp::Stamps);
        CHECK(lWork.Ops[0].FirstStamp == 0u);
        CHECK(lWork.Ops[0].StampCount == 3u);
        CHECK(lWork.Stamps[0].Center.x == 1.f);
        CHECK(lWork.Stamps[2].Center.x == 3.f);
        CHECK(lQueue.GetStampCount() == 3u);
    }

    TEST_CASE("a clear between two paint calls splits them, in call order")
    {
        Fixture        lFixture;
        RuntimeTexture lTile = lFixture.MakePaintable("/Runtime/Tile");

        PaintQueue lQueue;
        lQueue.AddStamps(lTile, { Stamp(1.f, 1.f) }, EPaintMode::Over);
        lQueue.AddClear(lTile, TextureRegion{ 0, 0, 8, 8 }, Vector4F{ 0.f, 0.f, 0.f, 0.f });
        lQueue.AddStamps(lTile, { Stamp(2.f, 2.f), Stamp(3.f, 3.f) }, EPaintMode::Over);

        REQUIRE(lQueue.GetWork().size() == 1u);
        const TDynArray<PaintOp>& lOps = lQueue.GetWork()[0].Ops;
        REQUIRE(lOps.size() == 3u);
        CHECK(lOps[0].Kind == EPaintOp::Stamps);
        CHECK(lOps[0].StampCount == 1u);
        CHECK(lOps[1].Kind == EPaintOp::Clear);
        CHECK(lOps[1].Region.Width == 8u);
        CHECK(lOps[2].Kind == EPaintOp::Stamps);
        CHECK(lOps[2].FirstStamp == 1u);
        CHECK(lOps[2].StampCount == 2u);
    }

    TEST_CASE("textures keep the order they were first painted in")
    {
        Fixture        lFixture;
        RuntimeTexture lA = lFixture.MakePaintable("/Runtime/A");
        RuntimeTexture lB = lFixture.MakePaintable("/Runtime/B");

        PaintQueue lQueue;
        lQueue.AddStamps(lB, { Stamp(1.f, 1.f) }, EPaintMode::Over);
        lQueue.AddStamps(lA, { Stamp(1.f, 1.f) }, EPaintMode::Over);
        lQueue.AddStamps(lB, { Stamp(2.f, 2.f) }, EPaintMode::Over);

        REQUIRE(lQueue.GetWork().size() == 2u);
        CHECK(lQueue.GetWork()[0].Target.GetPath() == OpaaxString("/Runtime/B"));
        CHECK(lQueue.GetWork()[0].Stamps.size() == 2u);
        CHECK(lQueue.GetWork()[1].Target.GetPath() == OpaaxString("/Runtime/A"));
    }

    TEST_CASE("stamps that draw nothing are dropped; none left adds nothing")
    {
        Fixture        lFixture;
        RuntimeTexture lTile = lFixture.MakePaintable("/Runtime/Tile");

        PaintStamp lFlat = Stamp(1.f, 1.f);
        lFlat.Size.y = 0.f;
        PaintStamp lLost = Stamp(1.f, 1.f);
        lLost.Center.x = TNumericLimits<float>::quiet_NaN();

        CHECK_FALSE(IsDrawableStamp(lFlat));
        CHECK_FALSE(IsDrawableStamp(lLost));
        CHECK(IsDrawableStamp(Stamp(1.f, 1.f)));

        PaintQueue lQueue;
        CHECK(lQueue.AddStamps(lTile, { lFlat, lLost }, EPaintMode::Over) == 2u);
        CHECK(lQueue.IsEmpty());

        CHECK(lQueue.AddStamps(lTile, { lFlat, Stamp(5.f, 5.f) }, EPaintMode::Over) == 1u);
        REQUIRE(lQueue.GetWork().size() == 1u);
        CHECK(lQueue.GetWork()[0].Ops[0].StampCount == 1u);

        lQueue.Reset();
        CHECK(lQueue.IsEmpty());
    }

    TEST_CASE("a clear rectangle is cut to the texture's texels; one wholly outside is nothing")
    {
        const TOptional<TextureRegion> lInside = ClipRegion(TextureRegion{ 2, 3, 4, 5 }, 64, 64);
        REQUIRE(lInside.has_value());
        CHECK(lInside->X == 2u);
        CHECK(lInside->Y == 3u);
        CHECK(lInside->Width == 4u);
        CHECK(lInside->Height == 5u);

        const TOptional<TextureRegion> lCut = ClipRegion(TextureRegion{ 60, 62, 10, 10 }, 64, 64);
        REQUIRE(lCut.has_value());
        CHECK(lCut->Width == 4u);
        CHECK(lCut->Height == 2u);

        CHECK_FALSE(ClipRegion(TextureRegion{ 64, 0, 4, 4 }, 64, 64).has_value());
        CHECK_FALSE(ClipRegion(TextureRegion{ 0, 0, 0, 4 }, 64, 64).has_value());
        CHECK_FALSE(ClipRegion(TextureRegion{ 0xFFFFFFF0u, 0, 0x20u, 4 }, 64, 64).has_value());
    }

    TEST_CASE("each stamp of a pass gets its own order in layer, rising with it")
    {
        CHECK(PaintOrderInLayer(0) == -32768);
        CHECK(PaintOrderInLayer(1) == -32767);
        CHECK(PaintOrderInLayer(MAX_STAMPS_PER_PASS - 1u) == 32767);

        // The next pass starts again from the bottom.
        CHECK(PaintOrderInLayer(MAX_STAMPS_PER_PASS) == -32768);
    }

    TEST_CASE("a paint pass: one unit per texel, its corners at the edges of the target")
    {
        const RenderView lView = MakePaintView(256, 128, EBlendMode::Paint);
        CHECK(lView.Blend == EBlendMode::Paint);
        CHECK_FALSE(lView.bLinearColor);
        CHECK(lView.Viewport.Width == 256u);
        CHECK(lView.Viewport.Height == 128u);

        const Vector4F lBottomLeft = lView.ViewProjection * Vector4F{ 0.f, 0.f, 0.f, 1.f };
        const Vector4F lTopRight   = lView.ViewProjection * Vector4F{ 256.f, 128.f, 0.f, 1.f };
        CHECK(lBottomLeft.x == doctest::Approx(-1.f));
        CHECK(lBottomLeft.y == doctest::Approx(-1.f));
        CHECK(lTopRight.x == doctest::Approx(1.f));
        CHECK(lTopRight.y == doctest::Approx(1.f));
    }

    TEST_CASE("only a paintable texture is painted or cleared; a clear wholly outside is refused")
    {
        Fixture        lFixture;
        RuntimeTexture lTile = lFixture.MakePaintable("/Runtime/Tile", 8, 8);

        const TDynArray<Uint8> lPixels(4u * 4u * 4u, 255u);
        RuntimeTexture lPlain = RuntimeTexture::Create(lFixture.Resources, OpaaxString("/Runtime/Plain"),
                                                       RuntimeTextureDesc{ 4, 4 }, lPixels.data());
        REQUIRE(lPlain.IsValid());

        CHECK(lTile.Paint({ Stamp(1.f, 1.f) }));
        CHECK(lTile.Clear());
        CHECK(lTile.ClearRegion(TextureRegion{ 6, 6, 4, 4 }));   // partly outside: cut
        CHECK_FALSE(lTile.ClearRegion(TextureRegion{ 8, 0, 2, 2 }));

        CHECK_FALSE(lPlain.Paint({ Stamp(1.f, 1.f) }));
        CHECK_FALSE(lPlain.Clear());
        CHECK_FALSE(RuntimeTexture{}.Paint({ Stamp(1.f, 1.f) }));
    }

    TEST_CASE("a batch's premultiplied slots: one bit for each slot holding a paintable texture")
    {
        FakeTexture lPlain(false);
        FakeTexture lPainted(true);

        CHECK(MakePremultipliedSlotMask({ &lPlain, &lPainted, nullptr, &lPainted }) == 0b1010u);
        CHECK(MakePremultipliedSlotMask({ &lPlain, &lPlain }) == 0u);
        CHECK(MakePremultipliedSlotMask({}) == 0u);
    }
}
