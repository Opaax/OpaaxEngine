// Suite: image sources (UI/UIImageSource.h) — UI25.
//
// One resolve serves UIImage and UIButton: a runtime pointer, then a sheet FRAME, then a texture.
// What is gated: the precedence; a frame's UVs reaching the quad (and, sliced, every quad landing
// inside the frame); "named but not ready" re-arming and "nothing named" not; and the new fields
// surviving the file. The pixels are theirs — a stub answers what the renderer's caches would.
#include <doctest.h>

#include "RHI/Texture.h"
#include "UI/UICanvas.h"
#include "UI/UICanvasFile.h"
#include "UI/UIImageSource.h"
#include "UI/UIWidgetRegistry.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    /** Never sampled — only its identity and size matter here. */
    class FakeTexture final : public ITexture2D
    {
    public:
        explicit FakeTexture(const Uint32 InSize = 8u) : m_Size(InSize) {}

        void   Bind(Uint32) const override {}
        void   Unbind()     const override {}
        Uint32 GetWidth()      const noexcept override { return m_Size; }
        Uint32 GetHeight()     const noexcept override { return m_Size; }
        Uint32 GetRendererID() const noexcept override { return 3u; }
        bool   IsLoaded()      const noexcept override { return true; }

    private:
        Uint32 m_Size;
    };

    /** A sheet of one 64x64 frame at (128, 64) of a 512x512 atlas, and a plain texture beside it. */
    class StubAssets final : public IUIAssetProvider
    {
    public:
        FakeTexture Atlas{ 512u };
        FakeTexture Plain{ 32u };
        bool        bSheetReady = true;
        Uint32      SheetResolves = 0;
        Int32       LastFrame     = -99;

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return &Plain; }

        UISheetFrameView ResolveSheetFrame(const char*, const Int32 InFrame) override
        {
            ++SheetResolves;
            LastFrame = InFrame;
            if (!bSheetReady) { return {}; }

            UISheetFrameView lView;
            lView.Texture = &Atlas;
            lView.UVMin   = { 0.25f, 0.75f };    // MakeFrameUV's V flip: the top row is the larger v
            lView.UVMax   = { 0.375f, 0.875f };
            lView.SizePx  = { 64.f, 64.f };
            return lView;
        }
    };

    UIWidgetRegistry MakeRegistry()
    {
        UIWidgetRegistry lRegistry;
        lRegistry.Register<UIPanel>(OPAAX_ID("UIPanel"));
        lRegistry.Register<UIImage>(OPAAX_ID("UIImage"));
        lRegistry.Register<UIButton>(OPAAX_ID("UIButton"));
        return lRegistry;
    }
}

TEST_CASE("UIImageSource: runtime beats sheet beats texture, and nothing named is a plain colour")
{
    StubAssets lAssets;
    UIBuildContext lContext{ &lAssets };

    TResourcePath<TextureResource>     lTexture;  lTexture.Path = "UI/Art.png";
    TResourcePath<SpriteSheetResource> lSheet;    lSheet.Path   = "UI/Icons.opaaxsheet";
    FakeTexture lRuntime{ 4u };

    UIResolvedImage lImage;
    bool            bNamed = false;

    CHECK(ResolveImageSource(lContext, &lRuntime, lTexture, lSheet, 2, lImage, bNamed));
    CHECK(lImage.Texture == &lRuntime);
    CHECK(lImage.SizePx.x == doctest::Approx(4.f));
    CHECK(lAssets.SheetResolves == 0);   // code handed a texture over: nobody is asked

    CHECK(ResolveImageSource(lContext, nullptr, lTexture, lSheet, 2, lImage, bNamed));
    CHECK(lImage.Texture == &lAssets.Atlas);
    CHECK(lImage.UVMin.x == doctest::Approx(0.25f));
    CHECK(lImage.SizePx.x == doctest::Approx(64.f));   // the FRAME's, not the atlas's
    CHECK(lAssets.LastFrame == 2);

    CHECK(ResolveImageSource(lContext, nullptr, lTexture, {}, -1, lImage, bNamed));
    CHECK(lImage.Texture == &lAssets.Plain);
    CHECK(lImage.UVMax.x == doctest::Approx(1.f));
    CHECK(lImage.SizePx.x == doctest::Approx(32.f));

    CHECK_FALSE(ResolveImageSource(lContext, nullptr, {}, {}, -1, lImage, bNamed));
    CHECK_FALSE(bNamed);

    // Named but not ready: false WITH bNamed — the widget's cue to re-arm rather than draw plain.
    lAssets.bSheetReady = false;
    CHECK_FALSE(ResolveImageSource(lContext, nullptr, {}, lSheet, 0, lImage, bNamed));
    CHECK(bNamed);
}

TEST_CASE("UIImage: a sheet frame reaches the quad's UVs, and a sliced frame stays inside it")
{
    StubAssets lAssets;
    UIBuildContext lContext{ &lAssets };

    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIImage* lImage = static_cast<UIImage*>(lCanvas.Root().AddChild(MakeUnique<UIImage>()));
    lImage->SetSheetFrame(OpaaxString("UI/Icons.opaaxsheet"), 5);
    lCanvas.Update(lContext);

    REQUIRE(lImage->GetQuads().size() == 1u);
    CHECK(lImage->GetQuads()[0].Texture == &lAssets.Atlas);
    CHECK(lImage->GetQuads()[0].UVMin.x == doctest::Approx(0.25f));
    CHECK(lImage->GetQuads()[0].UVMax.y == doctest::Approx(0.875f));
    CHECK(lAssets.LastFrame == 5);

    // Sliced: nine quads measured against the 64 px FRAME, every UV inside the frame's rect.
    lImage->SetBorder(UIMargin{ 8.f, 8.f, 8.f, 8.f });
    lCanvas.Update(lContext);
    REQUIRE(lImage->GetQuads().size() == 9u);
    for (const UIQuad& lQuad : lImage->GetQuads())
    {
        CHECK(lQuad.UVMin.x >= 0.25f - 1e-5f);
        CHECK(lQuad.UVMax.x <= 0.375f + 1e-5f);
        CHECK(lQuad.UVMin.y >= 0.75f - 1e-5f);
        CHECK(lQuad.UVMax.y <= 0.875f + 1e-5f);
    }

    // Not ready: nothing drawn, re-armed each frame (1 rebuild) until it lands, then idle.
    lAssets.bSheetReady = false;
    lImage->InvalidateContent();
    lCanvas.Update(lContext);
    CHECK(lImage->GetQuads().empty());
    CHECK(lCanvas.Update(lContext).Rebuilds == 1);

    lAssets.bSheetReady = true;
    CHECK(lCanvas.Update(lContext).Rebuilds == 1);
    CHECK(lImage->GetQuads().size() == 9u);
    CHECK(lCanvas.Update(lContext).Rebuilds == 0);
}

TEST_CASE("UIButton: its art resolves like an image's, tinted by the state colour")
{
    StubAssets lAssets;
    UIBuildContext lContext{ &lAssets };

    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIButton* lButton = static_cast<UIButton*>(lCanvas.Root().AddChild(MakeUnique<UIButton>()));
    lButton->Texture.Path = "UI/Button.png";
    lCanvas.Update(lContext);

    REQUIRE(lButton->GetQuads().size() == 1u);
    CHECK(lButton->GetQuads()[0].Texture == &lAssets.Plain);
    CHECK(lButton->GetQuads()[0].Color.r == doctest::Approx(lButton->Normal.r));

    lButton->Sheet.Path = "UI/Icons.opaaxsheet";
    lButton->Frame      = 1;
    lButton->InvalidateContent();
    lCanvas.Update(lContext);
    CHECK(lButton->GetQuads()[0].Texture == &lAssets.Atlas);   // the sheet wins
    CHECK(lButton->GetQuads()[0].UVMin.y == doctest::Approx(0.75f));

    // A plain button (nothing named) is still a coloured quad, not nothing.
    UIButton* lPlain = static_cast<UIButton*>(lCanvas.Root().AddChild(MakeUnique<UIButton>()));
    lCanvas.Update(lContext);
    REQUIRE(lPlain->GetQuads().size() == 1u);
    CHECK(lPlain->GetQuads()[0].Texture == nullptr);
}

TEST_CASE("UIImageSource: the new fields round-trip through the file, and a file without them reads")
{
    const UIWidgetRegistry lRegistry = MakeRegistry();

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = MakeUnique<UIPanel>();

    auto lImage = MakeUnique<UIImage>();
    lImage->Sheet.Path = "UI/Icons.opaaxsheet";
    lImage->Frame      = 7;
    lDoc.Root->AddChild(Move(lImage));

    auto lButton = MakeUnique<UIButton>();
    lButton->Texture.Path = "UI/Button.png";
    lButton->Sheet.Path   = "UI/Icons.opaaxsheet";
    lButton->Frame        = 2;
    lDoc.Root->AddChild(Move(lButton));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 2u);

    const auto* lReadImage  = static_cast<const UIImage*>(lBack.Root->GetChildren()[0].get());
    const auto* lReadButton = static_cast<const UIButton*>(lBack.Root->GetChildren()[1].get());
    CHECK(lReadImage->Sheet.Path == OpaaxString("UI/Icons.opaaxsheet"));
    CHECK(lReadImage->Frame == 7);
    CHECK(lReadButton->Texture.Path == OpaaxString("UI/Button.png"));
    CHECK(lReadButton->Sheet.Path == OpaaxString("UI/Icons.opaaxsheet"));
    CHECK(lReadButton->Frame == 2);

    UICanvasFile::UICanvasDoc lOld;
    REQUIRE(UICanvasFile::Deserialize(OpaaxString(R"({"Version":1,"Root":{"Type":"UIPanel","Children":[{"Type":"UIButton"}]}})"),
                                      lRegistry, lOld));
    const auto* lOldButton = static_cast<const UIButton*>(lOld.Root->GetChildren()[0].get());
    CHECK(lOldButton->Texture.IsEmpty());
    CHECK(lOldButton->Frame == -1);
}
