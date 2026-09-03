// Suite: the text stack's headless half — FontFaceData's lookups, FontBake's refusals, and the
// FontFaceResource placeholder.
//
// WHAT IS NOT HERE, and why it is not a hole: baking a REAL typeface needs a real `.ttf`, and this
// suite's standing rule is that it never reads the repo's assets ([[L20]] — the fixture is written
// into a temp dir, or it is bytes). Embedding a 45 KB font to prove stb_truetype packs is also the
// wrong instrument: what would fail is the vendor, not us. The real bake is gated by the smoke run's
// COUNTING log line — `'roboto-greek-400-normal' baked N glyph(s), atlas WxH, K kern pair(s)` — which
// discriminates because N and K are numbers a broken bake cannot produce ([[L15]], [[L59]]).
//
// What IS here is the part M5 got wrong and this rewrite exists to fix: the tables are keyed by
// CODEPOINT, not by `char - 0x20`. Every case below uses codepoints above 0xFF for exactly that
// reason — a Uint8-keyed table would pass an ASCII-only suite and lose every Greek glyph.
#include <doctest.h>

#include "Engine/Subsystems/Resources/ResourceManager.h"   // completes LoadContext
#include "Engine/Subsystems/Resources/Types/FontFaceResource.h"
#include "Renderer/Text/FontBake.h"
#include "Renderer/Text/FontFaceData.h"

using namespace Opaax;

namespace
{
    constexpr Uint32 GAMMA   = 0x0393u;   // Γ
    constexpr Uint32 ALPHA   = 0x03B1u;   // α
    constexpr Uint32 ZHE     = 0x0416u;   // Ж — a different script, so a Uint8 key would alias it
    constexpr Uint32 MISSING = 0x0041u;   // 'A', deliberately NOT in the faces built below

    /** A face carrying two Greek glyphs and one Cyrillic one. No atlas — none of this needs a GPU. */
    FontFaceData MakeFace()
    {
        FontFaceData lFace;
        lFace.PixelHeight  = 32.f;
        lFace.AtlasWidth   = 512u;
        lFace.AtlasHeight  = 512u;

        FontGlyph lGlyph;
        lGlyph.XAdvance = 17.f;
        lFace.Glyphs.emplace(GAMMA, lGlyph);

        lGlyph.XAdvance = 19.f;
        lFace.Glyphs.emplace(ALPHA, lGlyph);

        lGlyph.XAdvance = 21.f;
        lFace.Glyphs.emplace(ZHE, lGlyph);

        return lFace;
    }

    /** A context to hand Load. Every case here is a leaf load — nothing acquires a child. */
    struct LoadFixture
    {
        ResourceManager         Manager;
        ResourceDependencyGraph Deps;
        LoadContext             Ctx{ Manager, Deps };
    };
}

TEST_SUITE("FontFaceData")
{
    TEST_CASE("FindGlyph answers the glyph for a codepoint it has, and nullptr for one it does not")
    {
        const FontFaceData lFace = MakeFace();

        REQUIRE(lFace.FindGlyph(GAMMA) != nullptr);
        CHECK(lFace.FindGlyph(GAMMA)->XAdvance == doctest::Approx(17.f));
        CHECK(lFace.FindGlyph(ZHE)->XAdvance   == doctest::Approx(21.f));

        // NULLPTR, not a substitute: choosing what a miss looks like is the caller's decision, and a
        // face that quietly answered '?' would make "does this face cover this text?" unanswerable.
        CHECK(lFace.FindGlyph(MISSING) == nullptr);
        CHECK(lFace.GlyphCount() == 3u);
        CHECK_FALSE(lFace.IsEmpty());
    }

    TEST_CASE("GetKerning binary-searches a table sorted the way the bake sorts it")
    {
        FontFaceData lFace = MakeFace();

        // Inserted in the SAME order FontBake's std::sort produces — PackKey is shared by both sides
        // precisely so they cannot disagree, and a lookup into a differently-ordered array is the one
        // way a sorted-array search goes silently wrong.
        lFace.Kerning.emplace_back(FontKerningPair{ GAMMA, ALPHA, -1.5f });
        lFace.Kerning.emplace_back(FontKerningPair{ GAMMA, ZHE,   -0.5f });
        lFace.Kerning.emplace_back(FontKerningPair{ ZHE,   ALPHA,  2.0f });

        REQUIRE(FontFaceData::PackKey(GAMMA, ALPHA) < FontFaceData::PackKey(GAMMA, ZHE));
        REQUIRE(FontFaceData::PackKey(GAMMA, ZHE)   < FontFaceData::PackKey(ZHE,   ALPHA));

        CHECK(lFace.GetKerning(GAMMA, ALPHA) == doctest::Approx(-1.5f));
        CHECK(lFace.GetKerning(GAMMA, ZHE)   == doctest::Approx(-0.5f));
        CHECK(lFace.GetKerning(ZHE,   ALPHA) == doctest::Approx(2.0f));

        // An absent pair means zero — that is what lets the bake drop every unkerned pair.
        CHECK(lFace.GetKerning(ALPHA, GAMMA)   == doctest::Approx(0.f));
        CHECK(lFace.GetKerning(MISSING, GAMMA) == doctest::Approx(0.f));
    }

    TEST_CASE("GetKerning on an empty table is 0, not a walk off the front")
    {
        const FontFaceData lFace = MakeFace();

        REQUIRE(lFace.Kerning.empty());
        CHECK(lFace.GetKerning(GAMMA, ALPHA) == doctest::Approx(0.f));
    }

    TEST_CASE("PackKey orders by first codepoint, then second, across the whole 32-bit range")
    {
        // The ordering has to hold for codepoints a Uint8 key could not even represent — the exact
        // property M5's `(First << 8) | Second` packing lost the moment text stopped being ASCII.
        CHECK(FontFaceData::PackKey(0x0041u, 0xFFFFu) < FontFaceData::PackKey(0x0042u, 0x0000u));
        CHECK(FontFaceData::PackKey(GAMMA,   ALPHA)   < FontFaceData::PackKey(GAMMA,   ZHE));
    }
}

TEST_SUITE("FontBake")
{
    TEST_CASE("refuses a full-size buffer whose magic is not a font's, and does not fault reading it")
    {
        FontFaceData     lData = MakeFace();
        TDynArray<Uint8> lPixels{ 1u, 2u, 3u };

        // Long enough to clear the header-size guard, so this case reaches the MAGIC check — which is
        // the one that matters. stb_truetype answers -1 for an unrecognised tag, and handing that -1
        // to stbtt_InitFont indexes before the buffer: this case SEGFAULTED before the guard existed.
        // A `.ttf` that is really a PNG is an ordinary authoring mistake and must reach the
        // placeholder policy, not take the process down.
        TDynArray<Uint8> lNotAFont(64u, Uint8{ 0xAB });

        CHECK_FALSE(FontBake::Bake(lNotAFont.data(), lNotAFont.size(), FontBake::BakeParams{}, lData, lPixels));

        // A reload that fails must not half-replace the face the caller already had.
        CHECK(lData.GlyphCount() == 3u);
        CHECK(lPixels.size() == 3u);
    }

    TEST_CASE("refuses a buffer too short to hold a header, and never dereferences a null one")
    {
        FontFaceData     lData;
        TDynArray<Uint8> lPixels;

        const Uint8 lTooShort[] = { 'n', 'o', 't', ' ', 'a', ' ', 'f', 'o', 'n', 't' };

        CHECK_FALSE(FontBake::Bake(lTooShort, sizeof(lTooShort), FontBake::BakeParams{}, lData, lPixels));
        CHECK_FALSE(FontBake::Bake(nullptr, 0u, FontBake::BakeParams{}, lData, lPixels));
        CHECK_FALSE(FontBake::Bake(nullptr, 4096u, FontBake::BakeParams{}, lData, lPixels));
        CHECK(lData.IsEmpty());
    }

    TEST_CASE("the default scan window stops before CJK, which is the project's stated line")
    {
        const FontBake::BakeParams lParams;

        CHECK(lParams.FirstCodepoint == 0x0020u);
        CHECK(lParams.LastCodepoint  == 0x33FFu);

        // 0x3400 is CJK Unified Ideographs Extension A. Raising this bound would not make Chinese
        // work — it would overflow the atlas cap. That day needs a dynamic atlas, not a bigger number.
        CHECK(lParams.LastCodepoint < 0x3400u);
    }
}

TEST_SUITE("FontFaceResource")
{
    TEST_CASE("Placeholder is an empty face that can still LAY OUT")
    {
        const FontFaceResource lFace = FontFaceResource::Placeholder();

        // No glyphs, so every codepoint misses and the whole string draws tofu — the magenta-texture
        // rule applied to text.
        CHECK(lFace.Face.IsEmpty());
        CHECK(lFace.GetAtlas() == nullptr);
        CHECK_FALSE(lFace.IsUploaded());

        // But the metrics are usable: a zero PixelHeight divides by zero the moment a draw scales it,
        // and a zero LineAdvance stacks every line of tofu on top of itself.
        CHECK(lFace.Face.PixelHeight > 0.f);
        CHECK(lFace.Face.VMetrics.LineAdvance > 0.f);
        CHECK(lFace.Face.VMetrics.Ascent > 0.f);
        CHECK(lFace.Face.VMetrics.Descent < 0.f);
    }

    TEST_CASE("Load answers nullopt for a path that is not there")
    {
        LoadFixture lFixture;

        CHECK_FALSE(FontFaceResource::Load("Z:/nothing/here/Nope.ttf", lFixture.Ctx).has_value());
    }

    TEST_CASE("claims both desktop font spellings, and nothing else")
    {
        CHECK(FontFaceResource::Format.ExtensionCount == 2u);
        CHECK(OpaaxString(FontFaceResource::Format.Extensions[0]) == ".ttf");
        CHECK(OpaaxString(FontFaceResource::Format.Extensions[1]) == ".otf");
    }
}
