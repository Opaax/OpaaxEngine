// Suite: blend state — the factors of each blend mode, the maths they give, and painting into a
// premultiplied target then showing it. No device: what the GPU does with these factors is checked
// by TestWorld/Tests/PaintTexture.json.
#include <doctest.h>

#include "RHI/BlendState.h"

using namespace Opaax;

namespace
{
    bool Near(const Vector4F& InA, const Vector4F& InB, const float InTolerance = 1.0e-5f)
    {
        return Maths::Abs(InA.r - InB.r) <= InTolerance && Maths::Abs(InA.g - InB.g) <= InTolerance
            && Maths::Abs(InA.b - InB.b) <= InTolerance && Maths::Abs(InA.a - InB.a) <= InTolerance;
    }

    bool NearColor(const Vector4F& InA, const Vector4F& InB, const float InTolerance = 1.0e-5f)
    {
        return Maths::Abs(InA.r - InB.r) <= InTolerance && Maths::Abs(InA.g - InB.g) <= InTolerance
            && Maths::Abs(InA.b - InB.b) <= InTolerance;
    }

    const Vector4F TRANSPARENT_BLACK{ 0.f, 0.f, 0.f, 0.f };
    const Vector4F OPAQUE_WHITE{ 1.f, 1.f, 1.f, 1.f };
}

TEST_SUITE("BlendState")
{
    TEST_CASE("the factors of None, Alpha and Additive are the ones drawn with so far")
    {
        CHECK_FALSE(MakeBlendFactors(EBlendMode::None).bEnabled);

        const BlendFactors lAlpha = MakeBlendFactors(EBlendMode::Alpha);
        CHECK(lAlpha == BlendFactors{ true, EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha,
                                      EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha });

        const BlendFactors lAdditive = MakeBlendFactors(EBlendMode::Additive);
        CHECK(lAdditive == BlendFactors{ true, EBlendFactor::One, EBlendFactor::One, EBlendFactor::One, EBlendFactor::One });
    }

    TEST_CASE("Alpha into a transparent target: the colour is right, the alpha is squared")
    {
        const Vector4F lRed{ 1.f, 0.f, 0.f, 0.5f };

        // Over an opaque target the colour is the usual mix.
        CHECK(NearColor(ApplyBlend(MakeBlendFactors(EBlendMode::Alpha), lRed, Vector4F{ 0.f, 0.f, 1.f, 1.f }),
                        Vector4F{ 0.5f, 0.f, 0.5f, 1.f }));

        // Into a transparent one it keeps a quarter of the alpha: why painting has a mode of its own.
        CHECK(ApplyBlend(MakeBlendFactors(EBlendMode::Alpha), lRed, TRANSPARENT_BLACK).a == doctest::Approx(0.25f));
    }

    TEST_CASE("None: the source replaces the target")
    {
        const Vector4F lSource{ 0.2f, 0.4f, 0.6f, 0.5f };
        CHECK(Near(ApplyBlend(MakeBlendFactors(EBlendMode::None), lSource, OPAQUE_WHITE), lSource));
    }

    TEST_CASE("a premultiplied source over a transparent target is the source itself")
    {
        const Vector4F lSource = Premultiply(Vector4F{ 1.f, 0.f, 0.f, 0.5f });
        CHECK(Near(lSource, Vector4F{ 0.5f, 0.f, 0.f, 0.5f }));
        CHECK(Near(PremultipliedOver(lSource, TRANSPARENT_BLACK), lSource));
    }

    TEST_CASE("a premultiplied source over an opaque target mixes the colours and stays opaque")
    {
        const Vector4F lSource = Premultiply(Vector4F{ 1.f, 0.f, 0.f, 0.5f });
        CHECK(Near(PremultipliedOver(lSource, OPAQUE_WHITE), Vector4F{ 1.f, 0.5f, 0.5f, 1.f }));
    }

    TEST_CASE("Paint: a straight source over a transparent target leaves it premultiplied")
    {
        const Vector4F lRed{ 1.f, 0.f, 0.f, 0.5f };
        CHECK(Near(ApplyBlend(MakeBlendFactors(EBlendMode::Paint), lRed, TRANSPARENT_BLACK), Vector4F{ 0.5f, 0.f, 0.f, 0.5f }));
    }

    TEST_CASE("Paint: over an opaque target, Alpha's colour and an opaque alpha")
    {
        const Vector4F lRed{ 1.f, 0.f, 0.f, 0.5f };
        CHECK(Near(ApplyBlend(MakeBlendFactors(EBlendMode::Paint), lRed, OPAQUE_WHITE), Vector4F{ 1.f, 0.5f, 0.5f, 1.f }));
    }

    TEST_CASE("Paint is premultiplied source-over of the premultiplied source")
    {
        const Vector4F lSources[] = { { 1.f, 0.f, 0.f, 0.5f }, { 0.2f, 0.7f, 0.3f, 0.25f }, { 0.f, 0.f, 1.f, 1.f },
                                      { 0.9f, 0.9f, 0.1f, 0.f } };
        const Vector4F lTargets[] = { TRANSPARENT_BLACK, OPAQUE_WHITE, Premultiply(Vector4F{ 0.f, 0.5f, 1.f, 0.4f }),
                                      Premultiply(Vector4F{ 0.3f, 0.3f, 0.3f, 0.8f }) };

        for (const Vector4F& lSource : lSources)
        {
            for (const Vector4F& lTarget : lTargets)
            {
                CHECK(Near(ApplyBlend(MakeBlendFactors(EBlendMode::Paint), lSource, lTarget),
                           PremultipliedOver(Premultiply(lSource), lTarget)));
            }
        }
    }

    TEST_CASE("two stamps painted, then shown with Alpha, look like both drawn straight over the background")
    {
        const BlendFactors lPaint = MakeBlendFactors(EBlendMode::Paint);
        const BlendFactors lAlpha = MakeBlendFactors(EBlendMode::Alpha);

        const Vector4F lFirst{ 1.f, 0.f, 0.f, 0.6f };
        const Vector4F lSecond{ 0.f, 0.f, 1.f, 0.3f };
        const Vector4F lBackground{ 0.25f, 0.27f, 0.32f, 1.f };

        // Painted into a transparent texture, then drawn by a sprite that un-premultiplies it.
        const Vector4F lTexel = ApplyBlend(lPaint, lSecond, ApplyBlend(lPaint, lFirst, TRANSPARENT_BLACK));
        const Vector4F lShown = ApplyBlend(lAlpha, Unpremultiply(lTexel), lBackground);

        // The same stamps drawn as sprites straight over the background.
        const Vector4F lDirect = ApplyBlend(lAlpha, lSecond, ApplyBlend(lAlpha, lFirst, lBackground));

        CHECK(NearColor(lShown, lDirect));
    }

    TEST_CASE("Unpremultiply: the straight colour, black when transparent, never above 1")
    {
        CHECK(Near(Unpremultiply(Vector4F{ 0.5f, 0.f, 0.25f, 0.5f }), Vector4F{ 1.f, 0.f, 0.5f, 0.5f }));
        CHECK(Near(Unpremultiply(TRANSPARENT_BLACK), TRANSPARENT_BLACK));

        // 8-bit rounding can leave a colour above its alpha.
        CHECK(Unpremultiply(Vector4F{ 0.3f, 0.f, 0.f, 0.2f }).r == doctest::Approx(1.f));
    }
}
