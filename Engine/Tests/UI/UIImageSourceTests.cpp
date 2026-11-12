// Suite: image sources (UI/UIImageSource.h), used by UIImage and UIButton: runtime pointer, then
// sheet frame, then texture. Frame UVs, "named but not ready" retrying, and saving. A stub provider
// stands in for the renderer's caches.
#include <doctest.h>

#include "Core/Maths/Maths.h"
#include "Renderer/Textures/SpriteSheet.h"   // completes the widgets' sheet references
#include "RHI/Texture.h"
#include "UI/UICanvas.h"
#include "UI/UICanvasFile.h"
#include "UI/UIImageSource.h"
#include "Object/ObjectClassRegistry.h"
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
        Margin      FrameBorder;   // the frame's slice border, as the sheet would give it

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
            lView.Border  = FrameBorder;
            return lView;
        }
    };

    struct WidgetClasses final : ObjectClassRegistry
    {
        WidgetClasses()
        {
            Register<UIPanel>();
            Register<UIImage>();
            Register<UIButton>();
        }
    };
}

TEST_CASE("UIImageSource: runtime beats sheet beats texture, and nothing named is a plain colour")
{
    StubAssets lAssets;
    UIBuildContext lContext{ &lAssets };

    TAssetRef<Texture2D>   lTexture;  lTexture.Path = "UI/Art.png";
    TAssetRef<SpriteSheet> lSheet;    lSheet.Path   = "UI/Icons.opaaxasset";
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

TEST_CASE("UIImageSource: a sheet frame's slice border reaches the resolved image; a texture has none")
{
    StubAssets lAssets;
    lAssets.FrameBorder = Margin{ 20.f, 21.f, 22.f, 23.f };
    UIBuildContext lContext{ &lAssets };

    TAssetRef<Texture2D>   lTexture;  lTexture.Path = "UI/Art.png";
    TAssetRef<SpriteSheet> lSheet;    lSheet.Path   = "UI/Buttons.opaaxasset";
    FakeTexture lRuntime{ 4u };

    UIResolvedImage lImage;
    bool            bNamed = false;

    REQUIRE(ResolveImageSource(lContext, nullptr, {}, lSheet, 0, lImage, bNamed));
    CHECK(lImage.Border.Left == doctest::Approx(20.f));
    CHECK(lImage.Border.Right == doctest::Approx(21.f));
    CHECK(lImage.Border.Bottom == doctest::Approx(22.f));
    CHECK(lImage.Border.Top == doctest::Approx(23.f));

    // A texture, or one handed over by code, is not a sheet frame: no border, even after a frame.
    REQUIRE(ResolveImageSource(lContext, nullptr, lTexture, {}, -1, lImage, bNamed));
    CHECK(lImage.Border.IsZero());
    REQUIRE(ResolveImageSource(lContext, &lRuntime, {}, {}, -1, lImage, bNamed));
    CHECK(lImage.Border.IsZero());
}

TEST_CASE("UIImage: a sheet frame reaches the quad's UVs, and a sliced frame stays inside it")
{
    StubAssets lAssets;
    UIBuildContext lContext{ &lAssets };

    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIImage* lImage = static_cast<UIImage*>(lCanvas.Root().AddChild(NewObject<UIImage>()));
    lImage->SetSheetFrame(OpaaxString("UI/Icons.opaaxasset"), 5);
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

namespace
{
    /** A 200x100 image on a 1080 canvas showing frame 0 of the stub's sheet. */
    struct SlicedFixture
    {
        StubAssets     Assets;
        UIBuildContext Context{ &Assets };
        UICanvas       Canvas{ 1080.f };
        UIImage*       Image = nullptr;

        SlicedFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Image = static_cast<UIImage*>(Canvas.Root().AddChild(NewObject<UIImage>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 100.f };
            Image->SetRect(lRect);
            Image->SetSheetFrame(OpaaxString("UI/Buttons.opaaxasset"), 0);
        }

        const TDynArray<UIQuad>& Quads()
        {
            Canvas.Update(Context);
            return Image->GetQuads();
        }

        /** The width of the quad at the image's bottom-left corner. */
        float CornerWidth()
        {
            const TDynArray<UIQuad>& lQuads = Quads();
            const Vector2F           lMin   = Image->GetBounds().Min();
            for (const UIQuad& lQuad : lQuads)
            {
                if (Maths::Abs(lQuad.Bounds.Min().x - lMin.x) < 0.01f && Maths::Abs(lQuad.Bounds.Min().y - lMin.y) < 0.01f)
                {
                    return lQuad.Bounds.Size().x;
                }
            }
            return -1.f;
        }
    };
}

TEST_CASE("UIImage: with no border of its own, a bordered sheet frame is sliced, every UV inside the frame")
{
    SlicedFixture lF;
    lF.Assets.FrameBorder = Margin{ 8.f, 8.f, 8.f, 8.f };

    const TDynArray<UIQuad>& lQuads = lF.Quads();
    REQUIRE(lQuads.size() == 9u);
    for (const UIQuad& lQuad : lQuads)
    {
        CHECK(lQuad.Texture == &lF.Assets.Atlas);
        CHECK(lQuad.UVMin.x >= 0.25f - 1e-5f);
        CHECK(lQuad.UVMax.x <= 0.375f + 1e-5f);
        CHECK(lQuad.UVMin.y >= 0.75f - 1e-5f);
        CHECK(lQuad.UVMax.y <= 0.875f + 1e-5f);
    }
    CHECK(lF.CornerWidth() == doctest::Approx(8.f));
}

TEST_CASE("UIImage: its own border wins over the frame's; no border anywhere is one quad")
{
    SlicedFixture lF;
    lF.Assets.FrameBorder = Margin{ 8.f, 8.f, 8.f, 8.f };
    lF.Image->SetBorder(UIMargin{ 16.f, 16.f, 16.f, 16.f });
    CHECK(lF.CornerWidth() == doctest::Approx(16.f));

    lF.Image->SetBorder(UIMargin{});
    lF.Assets.FrameBorder = Margin{};
    lF.Image->InvalidateContent();
    REQUIRE(lF.Quads().size() == 1u);
    CHECK(lF.Quads()[0].UVMin.x == doctest::Approx(0.25f));
}

TEST_CASE("UIImage: BorderScale sizes the frame's corners, and a fill still cuts the nine quads")
{
    SlicedFixture lF;
    lF.Assets.FrameBorder = Margin{ 20.f, 20.f, 20.f, 20.f };

    lF.Image->SetBorderScale(0.5f);
    CHECK(lF.CornerWidth() == doctest::Approx(10.f));

    lF.Image->SetBorderScale(0.f);   // read as 1
    CHECK(lF.CornerWidth() == doctest::Approx(20.f));

    // Half filled: the left column whole, the middle one cut, the right one gone.
    lF.Image->SetFill(EUIFill::Horizontal, 0.5f);
    CHECK(lF.Quads().size() == 6u);
}

TEST_CASE("BuildImageQuads: appends after what OutQuads holds, and leaves it alone")
{
    UIResolvedImage lImage;
    FakeTexture     lTexture{ 64u };
    lImage.Texture = &lTexture;
    lImage.SizePx  = { 64.f, 64.f };
    lImage.Border  = Margin{ 16.f, 16.f, 16.f, 16.f };

    TDynArray<UIQuad> lQuads;
    UIQuad& lFirst = lQuads.emplace_back();
    lFirst.UVMax   = { 0.5f, 0.5f };
    lFirst.Color   = { 0.f, 1.f, 0.f, 1.f };

    BuildImageQuads(Bounds2D::FromMinMax({ 0.f, 0.f }, { 200.f, 100.f }), lImage, Margin{}, 1.f,
                    LinearColor{ 1.f, 0.f, 0.f, 1.f }, lQuads);

    REQUIRE(lQuads.size() == 10u);
    CHECK(lQuads[0].UVMax.x == doctest::Approx(0.5f));
    CHECK(lQuads[0].Color.g == doctest::Approx(1.f));
    CHECK(lQuads[0].Texture == nullptr);
    CHECK(lQuads[1].Texture == &lTexture);
    CHECK(lQuads[1].Color.r == doctest::Approx(1.f));
}

TEST_CASE("UIButton: its art resolves like an image's, tinted by the state colour")
{
    StubAssets lAssets;
    UIBuildContext lContext{ &lAssets };

    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIButton* lButton = static_cast<UIButton*>(lCanvas.Root().AddChild(NewObject<UIButton>()));
    lButton->Texture.Path = "UI/Button.png";
    lCanvas.Update(lContext);

    REQUIRE(lButton->GetQuads().size() == 1u);
    CHECK(lButton->GetQuads()[0].Texture == &lAssets.Plain);
    CHECK(lButton->GetQuads()[0].Color.r == doctest::Approx(lButton->Normal.r));

    lButton->Sheet.Path = "UI/Icons.opaaxasset";
    lButton->Frame      = 1;
    lButton->InvalidateContent();
    lCanvas.Update(lContext);
    CHECK(lButton->GetQuads()[0].Texture == &lAssets.Atlas);   // the sheet wins
    CHECK(lButton->GetQuads()[0].UVMin.y == doctest::Approx(0.75f));

    // A plain button (nothing named) is still a coloured quad, not nothing.
    UIButton* lPlain = static_cast<UIButton*>(lCanvas.Root().AddChild(NewObject<UIButton>()));
    lCanvas.Update(lContext);
    REQUIRE(lPlain->GetQuads().size() == 1u);
    CHECK(lPlain->GetQuads()[0].Texture == nullptr);
}

TEST_CASE("UIImageSource: the new fields round-trip through the file, and a file without them reads")
{
    const WidgetClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = NewObject<UIPanel>();

    auto lImage = NewObject<UIImage>();
    lImage->Sheet.Path  = "UI/Icons.opaaxasset";
    lImage->Frame       = 7;
    lImage->BorderScale = 0.4f;
    lDoc.Root->AddChild(Move(lImage));

    auto lButton = NewObject<UIButton>();
    lButton->Texture.Path = "UI/Button.png";
    lButton->Sheet.Path   = "UI/Icons.opaaxasset";
    lButton->Frame        = 2;
    lDoc.Root->AddChild(Move(lButton));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 2u);

    const auto* lReadImage  = static_cast<const UIImage*>(lBack.Root->GetChildren()[0].Get());
    const auto* lReadButton = static_cast<const UIButton*>(lBack.Root->GetChildren()[1].Get());
    CHECK(lReadImage->Sheet.Path == OpaaxString("UI/Icons.opaaxasset"));
    CHECK(lReadImage->Frame == 7);
    CHECK(lReadImage->BorderScale == doctest::Approx(0.4f));
    CHECK(lReadButton->Texture.Path == OpaaxString("UI/Button.png"));
    CHECK(lReadButton->Sheet.Path == OpaaxString("UI/Icons.opaaxasset"));
    CHECK(lReadButton->Frame == 2);

    UICanvasFile::UICanvasDoc lOld;
    REQUIRE(UICanvasFile::Deserialize(OpaaxString(R"({"Version":1,"Root":{"Type":"UIPanel","Children":[{"Type":"UIButton"},{"Type":"UIImage"}]}})"),
                                      lRegistry, lOld));
    const auto* lOldButton = static_cast<const UIButton*>(lOld.Root->GetChildren()[0].Get());
    CHECK(lOldButton->Texture.IsEmpty());
    CHECK(lOldButton->Frame == -1);
    const auto* lOldImage = static_cast<const UIImage*>(lOld.Root->GetChildren()[1].Get());
    CHECK(lOldImage->BorderScale == doctest::Approx(1.f));
}
