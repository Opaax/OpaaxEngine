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
#include "Engine/Subsystems/Resources/Types/FontFamilyData.h"
#include "Renderer/Text/FontBake.h"
#include "Renderer/Text/FontFaceData.h"
#include "Renderer/Text/Text2D.h"
#include "World/Components/TextComponent.h"

using namespace Opaax;

namespace
{
    constexpr Uint32 GAMMA   = 0x0393u;   // Γ
    constexpr Uint32 ALPHA   = 0x03B1u;   // α
    constexpr Uint32 ZHE     = 0x0416u;   // Ж — a different script, so a Uint8 key would alias it
    constexpr Uint32 MISSING = 0x0041u;   // 'A', deliberately NOT in the faces built below

    // The same three characters as UTF-8 BYTES, for the strings Measure walks. Explicit \x rather
    // than literal characters: this file has no BOM and the build sets no /utf-8, so a literal would
    // be decoded by the ANSI code page — the mechanism under test one layer down ([[L21]]).
    constexpr const char* U_GAMMA = "\xCE\x93";
    constexpr const char* U_ALPHA = "\xCE\xB1";

    constexpr float FACE_PIXEL_HEIGHT = 32.f;
    constexpr float FACE_LINE_ADVANCE = 32.f;
    constexpr float GAMMA_ADVANCE     = 17.f;
    constexpr float ALPHA_ADVANCE     = 19.f;
    constexpr float SPACE_ADVANCE     = 10.f;

    /** A face carrying two Greek glyphs, one Cyrillic one and a space. No atlas — no GPU needed. */
    FontFaceData MakeFace()
    {
        FontFaceData lFace;
        lFace.PixelHeight          = FACE_PIXEL_HEIGHT;
        lFace.AtlasWidth           = 512u;
        lFace.AtlasHeight          = 512u;
        lFace.VMetrics.Ascent      = 25.f;
        lFace.VMetrics.Descent     = -7.f;
        lFace.VMetrics.LineAdvance = FACE_LINE_ADVANCE;

        FontGlyph lGlyph;
        lGlyph.QuadSize = { 12.f, 20.f };

        lGlyph.XAdvance = GAMMA_ADVANCE;
        lFace.Glyphs.emplace(GAMMA, lGlyph);

        lGlyph.XAdvance = ALPHA_ADVANCE;
        lFace.Glyphs.emplace(ALPHA, lGlyph);

        lGlyph.XAdvance = 21.f;
        lFace.Glyphs.emplace(ZHE, lGlyph);

        // Blank, like a real space: it advances the pen and submits no quad.
        lGlyph.XAdvance = SPACE_ADVANCE;
        lGlyph.QuadSize = { 0.f, 0.f };
        lFace.Glyphs.emplace(static_cast<Uint32>(' '), lGlyph);

        return lFace;
    }

    /** One entry of a family, spelled out. */
    FontFamilyEntry MakeEntry(const EFontSubset InSubset, const EFontWeight InWeight,
                              const EFontWidth InWidth, const EFontSlant InSlant, const char* InPath)
    {
        FontFamilyEntry lEntry;
        lEntry.Style.Subset = InSubset;
        lEntry.Style.Weight = InWeight;
        lEntry.Style.Width  = InWidth;
        lEntry.Style.Slant  = InSlant;
        lEntry.Face.Path    = OpaaxString(InPath);

        return lEntry;
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
        CHECK(lFace.GlyphCount() == 4u);       // three letters and a space
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
        CHECK(lData.GlyphCount() == 4u);
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

TEST_SUITE("FontFamilyData")
{
    /** Latin at Regular and Bold, upright and italic; Greek at Regular upright only. */
    FontFamilyData MakeFamily()
    {
        using enum EFontSubset;

        FontFamilyData lFamily;
        lFamily.Entries.emplace_back(MakeEntry(Latin, EFontWeight::Regular, EFontWidth::Normal, EFontSlant::Normal, "latin-400.ttf"));
        lFamily.Entries.emplace_back(MakeEntry(Latin, EFontWeight::Regular, EFontWidth::Normal, EFontSlant::Italic, "latin-400i.ttf"));
        lFamily.Entries.emplace_back(MakeEntry(Latin, EFontWeight::Bold,    EFontWidth::Normal, EFontSlant::Normal, "latin-700.ttf"));
        lFamily.Entries.emplace_back(MakeEntry(Greek, EFontWeight::Regular, EFontWidth::Normal, EFontSlant::Normal, "greek-400.ttf"));

        return lFamily;
    }

    TEST_CASE("an exact request answers the exact face")
    {
        const FontFamilyData lFamily = MakeFamily();

        FontStyleKey lWanted;
        lWanted.Subset = EFontSubset::Latin;
        lWanted.Weight = EFontWeight::Bold;

        REQUIRE(lFamily.Find(lWanted) != nullptr);
        CHECK(lFamily.Find(lWanted)->Face.Path == "latin-700.ttf");
        CHECK(lFamily.FindExact(lWanted) != nullptr);
    }

    TEST_CASE("a weight the family lacks falls back to the NEAREST one it has")
    {
        const FontFamilyData lFamily = MakeFamily();

        FontStyleKey lWanted;
        lWanted.Subset = EFontSubset::Latin;
        lWanted.Weight = EFontWeight::SemiBold;   // 600: 100 from Bold, 200 from Regular

        CHECK(lFamily.FindExact(lWanted) == nullptr);
        REQUIRE(lFamily.Find(lWanted) != nullptr);
        CHECK(lFamily.Find(lWanted)->Face.Path == "latin-700.ttf");

        lWanted.Weight = EFontWeight::Medium;     // 500: 100 from Regular, 200 from Bold
        REQUIRE(lFamily.Find(lWanted) != nullptr);
        CHECK(lFamily.Find(lWanted)->Face.Path == "latin-400.ttf");
    }

    TEST_CASE("the SLANT outranks the weight — a Bold Italic request takes Regular Italic")
    {
        const FontFamilyData lFamily = MakeFamily();

        FontStyleKey lWanted;
        lWanted.Subset = EFontSubset::Latin;
        lWanted.Weight = EFontWeight::Bold;
        lWanted.Slant  = EFontSlant::Italic;

        // Bold upright is the right weight and Regular italic is the right slant. CSS resolves slant
        // first, so keeping the italic is correct — an upright "italic" reads as a bug, a slightly
        // light one reads as the font.
        REQUIRE(lFamily.Find(lWanted) != nullptr);
        CHECK(lFamily.Find(lWanted)->Face.Path == "latin-400i.ttf");
    }

    TEST_CASE("the WIDTH outranks the slant, which is CSS's own order")
    {
        FontFamilyData lFamily;
        lFamily.Entries.emplace_back(MakeEntry(EFontSubset::Latin, EFontWeight::Regular,
                                               EFontWidth::Normal, EFontSlant::Normal, "normal-upright.ttf"));
        lFamily.Entries.emplace_back(MakeEntry(EFontSubset::Latin, EFontWeight::Regular,
                                               EFontWidth::Condensed, EFontSlant::Italic, "condensed-italic.ttf"));

        FontStyleKey lWanted;
        lWanted.Subset = EFontSubset::Latin;
        lWanted.Width  = EFontWidth::Normal;
        lWanted.Slant  = EFontSlant::Italic;

        // Right width and wrong slant beats right slant and wrong width.
        REQUIRE(lFamily.Find(lWanted) != nullptr);
        CHECK(lFamily.Find(lWanted)->Face.Path == "normal-upright.ttf");
    }

    TEST_CASE("the SUBSET is never crossed — a script the family lacks answers nullptr")
    {
        const FontFamilyData lFamily = MakeFamily();

        FontStyleKey lWanted;
        lWanted.Subset = EFontSubset::Cyrillic;   // the family has Latin and Greek only

        // The one axis with no fallback, and the reason is that this one does not degrade: a Latin
        // face standing in for Cyrillic draws a screenful of tofu, so the honest miss is better.
        CHECK(lFamily.Find(lWanted) == nullptr);

        // ...while a Greek request at a weight it lacks still resolves, because that one degrades.
        lWanted.Subset = EFontSubset::Greek;
        lWanted.Weight = EFontWeight::Black;
        REQUIRE(lFamily.Find(lWanted) != nullptr);
        CHECK(lFamily.Find(lWanted)->Face.Path == "greek-400.ttf");
    }

    TEST_CASE("an empty family answers nullptr rather than reaching into nothing")
    {
        const FontFamilyData lFamily;

        CHECK(lFamily.EntryCount() == 0u);
        CHECK(lFamily.Find(FontStyleKey{}) == nullptr);
        CHECK(lFamily.FindExact(FontStyleKey{}) == nullptr);
    }
}

TEST_SUITE("Text2D::Measure")
{
    TEST_CASE("a single line is the sum of its advances, scaled by Size")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TextDrawParams lParams;
        lParams.Size     = FACE_PIXEL_HEIGHT;   // scale 1
        lParams.bKerning = false;

        const OpaaxString lText = OpaaxString(U_GAMMA) + U_ALPHA;
        const Vector2F    lSize = Text2D::Measure(lText.CStr(), lView, lParams);

        CHECK(lSize.x == doctest::Approx(GAMMA_ADVANCE + ALPHA_ADVANCE));
        CHECK(lSize.y == doctest::Approx(FACE_LINE_ADVANCE));

        // Size is ABSOLUTE, so doubling it doubles both extents — the property an author relies on.
        lParams.Size = FACE_PIXEL_HEIGHT * 2.f;
        const Vector2F lDouble = Text2D::Measure(lText.CStr(), lView, lParams);

        CHECK(lDouble.x == doctest::Approx(lSize.x * 2.f));
        CHECK(lDouble.y == doctest::Approx(lSize.y * 2.f));
    }

    TEST_CASE("kerning narrows the pair, and switching it off restores the plain advance")
    {
        FontFaceData lFace = MakeFace();
        lFace.Kerning.emplace_back(FontKerningPair{ GAMMA, ALPHA, -1.5f });

        const FontFaceView lView{ &lFace, nullptr };
        const OpaaxString  lText = OpaaxString(U_GAMMA) + U_ALPHA;

        TextDrawParams lParams;
        lParams.Size = FACE_PIXEL_HEIGHT;

        lParams.bKerning = true;
        CHECK(Text2D::Measure(lText.CStr(), lView, lParams).x
              == doctest::Approx(GAMMA_ADVANCE + ALPHA_ADVANCE - 1.5f));

        lParams.bKerning = false;
        CHECK(Text2D::Measure(lText.CStr(), lView, lParams).x
              == doctest::Approx(GAMMA_ADVANCE + ALPHA_ADVANCE));
    }

    TEST_CASE("'\\n' starts a line, and the width is the WIDEST one rather than the last")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TextDrawParams lParams;
        lParams.Size     = FACE_PIXEL_HEIGHT;
        lParams.bKerning = false;

        // "α α\nΓ" — the FIRST line is the wide one, so a walker that reported the last would fail.
        const OpaaxString lText = OpaaxString(U_ALPHA) + " " + U_ALPHA + "\n" + U_GAMMA;
        const Vector2F    lSize = Text2D::Measure(lText.CStr(), lView, lParams);

        CHECK(lSize.x == doctest::Approx(ALPHA_ADVANCE + SPACE_ADVANCE + ALPHA_ADVANCE));
        CHECK(lSize.y == doctest::Approx(FACE_LINE_ADVANCE * 2.f));

        lParams.LineHeightScale = 1.5f;
        CHECK(Text2D::Measure(lText.CStr(), lView, lParams).y
              == doctest::Approx(FACE_LINE_ADVANCE * 2.f * 1.5f));
    }

    TEST_CASE("'\\t' advances four spaces")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TextDrawParams lParams;
        lParams.Size = FACE_PIXEL_HEIGHT;

        CHECK(Text2D::Measure("\t", lView, lParams).x == doctest::Approx(SPACE_ADVANCE * 4.f));
    }

    TEST_CASE("a missing codepoint still advances, proportionally to Size")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        REQUIRE(lFace.FindGlyph(MISSING) == nullptr);

        TextDrawParams lParams;
        lParams.Size = FACE_PIXEL_HEIGHT;

        const float lOne = Text2D::Measure("A", lView, lParams).x;

        // The RATIO is the source's business; what this pins is that a tofu box occupies real width
        // (a zero would pile the whole string on one spot) and that three cost exactly three.
        CHECK(lOne > 0.f);
        CHECK(Text2D::Measure("AAA", lView, lParams).x == doctest::Approx(lOne * 3.f));

        lParams.Size = FACE_PIXEL_HEIGHT * 2.f;
        CHECK(Text2D::Measure("A", lView, lParams).x == doctest::Approx(lOne * 2.f));
    }

    TEST_CASE("nothing to measure is {0,0}, never a division by the face's zero height")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lValid{ &lFace, nullptr };

        CHECK(Text2D::Measure(nullptr, lValid).x == doctest::Approx(0.f));
        CHECK(Text2D::Measure("",      lValid).y == doctest::Approx(0.f));

        // No face named at all.
        CHECK(Text2D::Measure("A", FontFaceView{}).x == doctest::Approx(0.f));

        // A value-initialised face has PixelHeight 0 — the divide FontFaceData::Tofu exists to avoid.
        const FontFaceData lZero;
        CHECK(Text2D::Measure("A", FontFaceView{ &lZero, nullptr }).x == doctest::Approx(0.f));

        // ...and the tofu face, which is what a failed load resolves to, measures normally.
        const FontFaceData lTofu = FontFaceData::Tofu();
        CHECK(Text2D::Measure("A", FontFaceView{ &lTofu, nullptr }).x > 0.f);
    }
}

TEST_SUITE("Text2D::Layout")
{
    // The sink is what lets ONE walk feed two renderers — the viewport through Renderer2D and the
    // editor's font preview through ImGui. What a sink receives is therefore a contract, and the
    // editor's half cannot be tested (it needs ImGui), so the QUADS are tested here instead.

    TEST_CASE("emits one quad per VISIBLE glyph, in reading order, and none for a space")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TDynArray<TextQuad> lQuads;

        // "Γ αΓ" — four codepoints, one of them a blank space glyph.
        const OpaaxString lText = OpaaxString(U_GAMMA) + " " + U_ALPHA + U_GAMMA;

        Text2D::Layout(lText.CStr(), { 0.f, 0.f }, lView, TextDrawParams{},
                       [&lQuads](const TextQuad& InQuad) { lQuads.emplace_back(InQuad); });

        REQUIRE(lQuads.size() == 3u);

        // Reading order, left to right — a sink that drew them in any other order would still LOOK
        // right for one glyph and wrong for a kerned pair.
        CHECK(lQuads[0].Centre.x < lQuads[1].Centre.x);
        CHECK(lQuads[1].Centre.x < lQuads[2].Centre.x);

        for (const TextQuad& lQuad : lQuads)
        {
            CHECK_FALSE(lQuad.bTofu);
            CHECK(lQuad.Size.x > 0.f);
            CHECK(lQuad.Size.y > 0.f);
        }
    }

    TEST_CASE("the origin is the TOP-LEFT and Y goes UP, so every glyph sits below it")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TDynArray<TextQuad> lQuads;

        // The one sign the whole layout turns on, and the one a y-down sink has to negate. A sink
        // that got this backwards would draw the string above its box, off-panel.
        Text2D::Layout(U_GAMMA, { 0.f, 0.f }, lView, TextDrawParams{},
                       [&lQuads](const TextQuad& InQuad) { lQuads.emplace_back(InQuad); });

        REQUIRE(lQuads.size() == 1u);
        CHECK(lQuads[0].Centre.x > 0.f);
        CHECK(lQuads[0].Centre.y < 0.f);
    }

    TEST_CASE("a newline drops the next glyph by one line step")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TDynArray<TextQuad> lQuads;

        const OpaaxString lText = OpaaxString(U_GAMMA) + "\n" + U_GAMMA;

        TextDrawParams lParams;
        lParams.Size = FACE_PIXEL_HEIGHT;   // scale 1, so the step is the face's own advance

        Text2D::Layout(lText.CStr(), { 0.f, 0.f }, lView, lParams,
                       [&lQuads](const TextQuad& InQuad) { lQuads.emplace_back(InQuad); });

        REQUIRE(lQuads.size() == 2u);
        CHECK(lQuads[1].Centre.x == doctest::Approx(lQuads[0].Centre.x));
        CHECK(lQuads[0].Centre.y - lQuads[1].Centre.y == doctest::Approx(FACE_LINE_ADVANCE));
    }

    TEST_CASE("a missing codepoint emits a TOFU quad, and it carries no atlas rectangle")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TDynArray<TextQuad> lQuads;

        Text2D::Layout("A", { 0.f, 0.f }, lView, TextDrawParams{},
                       [&lQuads](const TextQuad& InQuad) { lQuads.emplace_back(InQuad); });

        REQUIRE(lQuads.size() == 1u);
        CHECK(lQuads[0].bTofu);
        CHECK(lQuads[0].Size.x > 0.f);

        // A sink must not sample the atlas for one of these — there is no glyph behind it.
        CHECK(lQuads[0].UVMin.x == doctest::Approx(0.f));
        CHECK(lQuads[0].UVMax.x == doctest::Approx(0.f));
    }

    TEST_CASE("an EMPTY sink answers exactly what Measure answers — they are one walk")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        const OpaaxString lText = OpaaxString(U_GAMMA) + U_ALPHA + "\n" + U_ALPHA;

        const Vector2F lMeasured = Text2D::Measure(lText.CStr(), lView);
        const Vector2F lLaidOut  = Text2D::Layout(lText.CStr(), { 0.f, 0.f }, lView, TextDrawParams{}, {});

        CHECK(lLaidOut.x == doctest::Approx(lMeasured.x));
        CHECK(lLaidOut.y == doctest::Approx(lMeasured.y));
    }
}

TEST_SUITE("Text2D::EstimateExtent")
{
    // The extent EntityQuery uses for PICKING and for the selection outline, computed with no face
    // to ask. Its contract is not accuracy, it is "never short" — a box that comes up small makes
    // the tail of a string unclickable, which reads as a broken entity rather than as a near miss.

    TEST_CASE("never comes up SHORT of the real measurement — the whole contract")
    {
        const FontFaceData lFace = MakeFace();
        const FontFaceView lView{ &lFace, nullptr };

        TextDrawParams lParams;
        lParams.Size     = FACE_PIXEL_HEIGHT;
        lParams.bKerning = false;

        const OpaaxString lLine  = OpaaxString(U_GAMMA) + U_ALPHA + " " + U_ALPHA;
        const OpaaxString lMulti = lLine + "\n" + lLine + "\n" + U_GAMMA;

        for (const OpaaxString* lText : { &lLine, &lMulti })
        {
            CAPTURE(lText->CStr());

            const Vector2F lMeasured  = Text2D::Measure(lText->CStr(), lView, lParams);
            const Vector2F lEstimated = Text2D::EstimateExtent(lText->CStr(), lParams);

            CHECK(lEstimated.x >= lMeasured.x);
            CHECK(lEstimated.y >= lMeasured.y);
        }
    }

    TEST_CASE("counts CODEPOINTS, not bytes — a Greek string is not three times as wide")
    {
        TextDrawParams lParams;
        lParams.Size = FACE_PIXEL_HEIGHT;

        // "Γα" is two codepoints in four bytes. A byte count would double the box.
        const OpaaxString lGreek = OpaaxString(U_GAMMA) + U_ALPHA;

        CHECK(Text2D::EstimateExtent(lGreek.CStr(), lParams).x
              == doctest::Approx(Text2D::EstimateExtent("ab", lParams).x));
    }

    TEST_CASE("scales with Size, counts lines, and answers zero for nothing")
    {
        TextDrawParams lParams;
        lParams.Size = FACE_PIXEL_HEIGHT;

        const float lOneLine = Text2D::EstimateExtent("abc", lParams).y;
        CHECK(Text2D::EstimateExtent("abc\ndef", lParams).y == doctest::Approx(lOneLine * 2.f));

        lParams.Size = FACE_PIXEL_HEIGHT * 2.f;
        CHECK(Text2D::EstimateExtent("abc", lParams).x
              == doctest::Approx(Text2D::EstimateExtent("abc", TextDrawParams{}).x * 2.f));

        CHECK(Text2D::EstimateExtent(nullptr).x == doctest::Approx(0.f));
        CHECK(Text2D::EstimateExtent("").y      == doctest::Approx(0.f));
    }
}

TEST_SUITE("TextComponent")
{
    // What AUTHORING a string has to survive. The Inspector's field is multiline as of 2026-09-04,
    // so a '\n' is now something a user can type — and a component that lost it on save would lose
    // it silently, at the one moment nobody is looking.

    TEST_CASE("a multi-line, multi-script string round-trips through the component's json")
    {
        TextComponent lWritten;
        lWritten.Text  = OpaaxString("Opaax\n") + U_GAMMA + U_ALPHA + "\t" + U_ALPHA + "\nend";
        lWritten.Size  = 48.f;
        lWritten.Style.Subset = EFontSubset::Greek;
        lWritten.Style.Weight = EFontWeight::Medium;
        lWritten.Style.Slant  = EFontSlant::Italic;
        lWritten.Font.Path    = OpaaxString("/Engine/Fonts/Roboto.opaaxfont");

        const nlohmann::json lJson = lWritten;
        const TextComponent  lRead = lJson.get<TextComponent>();

        // The escapes are the point: '\n' and '\t' are the two characters the layout walker acts on,
        // and json is where they would quietly become spaces.
        CHECK(lRead.Text == lWritten.Text);
        CHECK(lRead.Text.Find("\n") >= 0);
        CHECK(lRead.Text.Find("\t") >= 0);

        // The style is four enums written as LABELS, so a reordered enum cannot silently repoint them.
        CHECK(lRead.Style.Subset == EFontSubset::Greek);
        CHECK(lRead.Style.Weight == EFontWeight::Medium);
        CHECK(lRead.Style.Slant  == EFontSlant::Italic);
        CHECK(lRead.Size == doctest::Approx(48.f));
        CHECK(lRead.Font.Path == "/Engine/Fonts/Roboto.opaaxfont");
    }

    TEST_CASE("a map written before this component existed still reads — _WITH_DEFAULT, not at()")
    {
        // The empty object is the shape an older `.opaaxmap` presents: every key absent. The plain
        // NLOHMANN macro would THROW here, at boot, inside Level::MountAll.
        const TextComponent lRead = nlohmann::json::object().get<TextComponent>();

        CHECK(lRead.Text == "Text");
        CHECK(lRead.Font.IsEmpty());
        CHECK(lRead.Face.IsEmpty());
        CHECK(lRead.bVisible);
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
