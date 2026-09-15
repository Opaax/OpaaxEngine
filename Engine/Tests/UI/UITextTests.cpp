// Suite: UIText (UI/Widgets/UIText.h) — a string in a rect, through Text2D's one walk.
//
// The face is hand-built (no .ttf, no GPU) and handed over by a stub provider, which is exactly
// the seam the widget sees at runtime: a view whose atlas may still be null.
#include <doctest.h>

#include "Renderer/Text/FontFaceData.h"
#include "RHI/Texture.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace
{
    constexpr float PIXEL_HEIGHT = 32.f;
    constexpr float LINE_ADVANCE = 40.f;
    constexpr float GLYPH_ADV    = 10.f;
    constexpr float SPACE_ADV    = 5.f;

    /** 'a' and a blank space, nothing else — a Latin string is then all 'a's, 10 wide each. */
    FontFaceData MakeFace()
    {
        FontFaceData lFace;
        lFace.PixelHeight          = PIXEL_HEIGHT;
        lFace.AtlasWidth           = 64u;
        lFace.AtlasHeight          = 64u;
        lFace.VMetrics.Ascent      = 28.f;
        lFace.VMetrics.Descent     = -8.f;
        lFace.VMetrics.LineAdvance = LINE_ADVANCE;

        FontGlyph lGlyph;
        lGlyph.QuadSize = { 8.f, 20.f };
        lGlyph.XAdvance = GLYPH_ADV;
        lFace.Glyphs.emplace(static_cast<Uint32>('a'), lGlyph);

        lGlyph.XAdvance = SPACE_ADV;
        lGlyph.QuadSize = { 0.f, 0.f };
        lFace.Glyphs.emplace(static_cast<Uint32>(' '), lGlyph);

        return lFace;
    }

    /** Never sampled — the widget only stores the pointer. Non-null is all "uploaded" means here. */
    class FakeAtlas final : public ITexture2D
    {
    public:
        void   Bind(Uint32) const override {}
        void   Unbind()     const override {}
        Uint32 GetWidth()      const noexcept override { return 64u; }
        Uint32 GetHeight()     const noexcept override { return 64u; }
        Uint32 GetRendererID() const noexcept override { return 1u; }
        bool   IsLoaded()      const noexcept override { return true; }
    };

    class StubFonts final : public IUIAssetProvider
    {
    public:
        FontFaceData Face  = MakeFace();
        FakeAtlas    Atlas;
        bool         bUploaded = true;
        Uint32       Resolves  = 0;
        OpaaxString  LastPath;

        FontFaceView ResolveFace(const char* InAssetPath) override
        {
            ++Resolves;
            LastPath = InAssetPath;
            return FontFaceView{ &Face, bUploaded ? &Atlas : nullptr };
        }

        ITexture2D* ResolveTexture(const char*) override { return nullptr; }
    };

    struct Fixture
    {
        UICanvas  Canvas{ 1080.f };
        StubFonts Fonts;
        UIText*   Text = nullptr;

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Text = static_cast<UIText*>(Canvas.Root().AddChild(MakeUnique<UIText>()));
            Text->Font = "/Fonts/fake.ttf";
            Text->Size = PIXEL_HEIGHT;   // scale 1
        }

        UICanvasStats Update() { return Canvas.Update(UIBuildContext{ &Fonts }); }
    };
}

TEST_CASE("UIText: one quad per visible glyph, on the atlas, starting at the rect's top-left")
{
    Fixture lF;
    UIRect lRect;
    lRect.SizeDelta = { 400.f, 100.f };   // centred: x in [-200, 200], y in [-50, 50]
    lF.Text->SetRect(lRect);
    lF.Text->SetText("a a");

    lF.Update();

    REQUIRE(lF.Text->GetQuads().size() == 2u);
    const UIQuad& lFirst = lF.Text->GetQuads()[0];
    CHECK(lFirst.Texture == &lF.Fonts.Atlas);
    CHECK(lFirst.Outline == doctest::Approx(0.f));
    CHECK(lFirst.Bounds.Min().x == doctest::Approx(-200.f));   // pen at the left edge, no offset
    CHECK(lFirst.Bounds.Max().y < 50.f);                        // below the top edge
    CHECK(lF.Fonts.LastPath == OpaaxString("/Fonts/fake.ttf"));
}

TEST_CASE("UIText: Right ends the line on the rect's right edge; Middle centres the line box vertically")
{
    Fixture lF;
    UIRect lRect;
    lRect.SizeDelta = { 400.f, 100.f };
    lF.Text->SetRect(lRect);
    lF.Text->SetText("aa");   // 20 wide
    lF.Text->SetAlign(ETextAlign::Right, EUIVAlign::Top);
    lF.Update();

    REQUIRE(lF.Text->GetQuads().size() == 2u);
    // The pen for the second glyph sits at 200 - 10; its quad starts there (QuadOffset 0).
    CHECK(lF.Text->GetQuads()[1].Bounds.Min().x == doctest::Approx(200.f - GLYPH_ADV));

    lF.Text->SetAlign(ETextAlign::Left, EUIVAlign::Middle);
    lF.Update();

    // One line box of 40 in a 100 rect: shifted down by 30 from the Top placement.
    const float lTopY = 50.f - 28.f - (0.f + 20.f * 0.5f);   // baseline below the top, then the glyph's centre
    CHECK(lF.Text->GetQuads()[0].Bounds.Center.y == doctest::Approx(lTopY - 30.f));
}

TEST_CASE("UIText: wraps at the rect's width by default, and not when told not to")
{
    Fixture lF;
    UIRect lRect;
    lRect.SizeDelta = { 25.f, 200.f };   // room for "aa" (20) but not "aa aa"
    lF.Text->SetRect(lRect);
    lF.Text->SetText("aa aa");
    lF.Update();

    REQUIRE(lF.Text->GetQuads().size() == 4u);
    CHECK(lF.Text->GetQuads()[0].Bounds.Center.y - lF.Text->GetQuads()[2].Bounds.Center.y == doctest::Approx(LINE_ADVANCE));

    lF.Text->bWrap = false;
    lF.Text->InvalidateContent();
    lF.Update();

    CHECK(lF.Text->GetQuads()[0].Bounds.Center.y == doctest::Approx(lF.Text->GetQuads()[2].Bounds.Center.y));
}

TEST_CASE("UIText: an atlas still uploading draws nothing and re-arms; the frame it lands, it draws")
{
    Fixture lF;
    lF.Text->SetText("a");
    lF.Fonts.bUploaded = false;

    lF.Update();
    CHECK(lF.Text->GetQuads().empty());
    CHECK(lF.Fonts.Resolves == 1u);

    UICanvasStats lStats = lF.Update();
    CHECK(lStats.Rebuilds == 1);   // asked again, nobody polled
    CHECK(lF.Fonts.Resolves == 2u);

    lF.Fonts.bUploaded = true;
    lF.Update();
    CHECK(lF.Text->GetQuads().size() == 1u);

    lStats = lF.Update();
    CHECK(lStats.Rebuilds == 0);   // and now idle
}

TEST_CASE("UIText: no provider, no font, or no text draws nothing and does not re-arm")
{
    Fixture lF;
    lF.Text->SetText("a");

    UICanvasStats lStats = lF.Canvas.Update(UIBuildContext{});   // no provider
    CHECK(lF.Text->GetQuads().empty());
    lStats = lF.Canvas.Update(UIBuildContext{});
    CHECK(lStats.Rebuilds == 0);

    lF.Text->SetFont("");
    lF.Update();
    CHECK(lF.Text->GetQuads().empty());
    CHECK(lF.Fonts.Resolves == 0u);   // never even asked

    lF.Text->SetFont("/Fonts/fake.ttf");
    lF.Text->SetText("");
    lF.Update();
    CHECK(lF.Text->GetQuads().empty());
}

TEST_CASE("UIText: a codepoint the face lacks is a hollow box on no atlas")
{
    Fixture lF;
    lF.Text->SetText("Z");
    lF.Update();

    REQUIRE(lF.Text->GetQuads().size() == 1u);
    CHECK(lF.Text->GetQuads()[0].Outline > 0.f);
    CHECK(lF.Text->GetQuads()[0].Texture == nullptr);
}
