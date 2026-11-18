// Suite: brushes (UI/UIBrush.h) — what fills a rect: runtime texture, then sheet frame, then texture;
// named but not ready draws nothing; nothing named is a plain tinted quad. UIImage and each state of
// UIButton draw the brush their fields make. A stub provider stands in for the renderer's caches.
#include <doctest.h>

#include "RHI/Texture.h"
#include "UI/UIBrush.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"

using namespace Opaax;

namespace
{
    /** Never sampled: only its identity and size matter. */
    class FakeTexture final : public ITexture2D
    {
    public:
        explicit FakeTexture(const Uint32 InSize = 8u) : m_Size(InSize) {}

        void   Bind(Uint32) const override {}
        void   Unbind()     const override {}
        Uint32 GetWidth()      const noexcept override { return m_Size; }
        Uint32 GetHeight()     const noexcept override { return m_Size; }
        Uint32 GetRendererID() const noexcept override { return 5u; }
        bool   IsLoaded()      const noexcept override { return true; }

    private:
        Uint32 m_Size;
    };

    /** A sheet whose frame N sits at u = 0.1 * N, 64 px with a 16 px border, and a plain texture. */
    class StubAssets final : public IUIAssetProvider
    {
    public:
        FakeTexture Atlas{ 512u };
        FakeTexture Plain{ 32u };
        bool        bSheetReady = true;
        bool        bTextureReady = true;
        Int32       LastFrame = -99;
        ESliceMode  FrameMode = ESliceMode::Stretch;

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return bTextureReady ? &Plain : nullptr; }

        UISheetFrameView ResolveSheetFrame(const char*, const Int32 InFrame) override
        {
            LastFrame = InFrame;
            if (!bSheetReady) { return {}; }

            const float lU = 0.1f * static_cast<float>(InFrame < 0 ? 0 : InFrame);
            UISheetFrameView lView;
            lView.Texture   = &Atlas;
            lView.UVMin     = { lU, 0.5f };
            lView.UVMax     = { lU + 0.05f, 0.625f };
            lView.SizePx    = { 64.f, 64.f };
            lView.Border    = Margin{ 16.f, 16.f, 16.f, 16.f };
            lView.SliceMode = FrameMode;
            return lView;
        }
    };

    const Bounds2D BRUSH_RECT = Bounds2D::FromMinMax({ 0.f, 0.f }, { 200.f, 100.f });

    UIBrush SheetBrush(const Int32 InFrame)
    {
        UIBrush lBrush;
        lBrush.Sheet.Path = "UI/Buttons.opaaxasset";
        lBrush.Frame      = InFrame;
        return lBrush;
    }

    void CheckSameQuads(const TDynArray<UIQuad>& InA, const TDynArray<UIQuad>& InB)
    {
        REQUIRE(InA.size() == InB.size());
        for (Uint64 lIndex = 0; lIndex < InA.size(); ++lIndex)
        {
            CAPTURE(lIndex);
            CHECK(InA[lIndex].Bounds.Min().x == doctest::Approx(InB[lIndex].Bounds.Min().x));
            CHECK(InA[lIndex].Bounds.Min().y == doctest::Approx(InB[lIndex].Bounds.Min().y));
            CHECK(InA[lIndex].Bounds.Size().x == doctest::Approx(InB[lIndex].Bounds.Size().x));
            CHECK(InA[lIndex].Bounds.Size().y == doctest::Approx(InB[lIndex].Bounds.Size().y));
            CHECK(InA[lIndex].UVMin.x == doctest::Approx(InB[lIndex].UVMin.x));
            CHECK(InA[lIndex].UVMax.y == doctest::Approx(InB[lIndex].UVMax.y));
            CHECK(InA[lIndex].Color.r == doctest::Approx(InB[lIndex].Color.r));
            CHECK(InA[lIndex].Color.a == doctest::Approx(InB[lIndex].Color.a));
            CHECK(InA[lIndex].Texture == InB[lIndex].Texture);
        }
    }
}

// =============================================================================
// Resolving
// =============================================================================
TEST_CASE("UIBrush: a runtime texture beats the sheet, the sheet beats the texture")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };
    FakeTexture    lRuntime{ 4u };

    UIBrush lBrush = SheetBrush(2);
    lBrush.Texture.Path = "UI/Art.png";

    UIResolvedImage lImage;
    bool            bNamed = false;

    CHECK(ResolveBrush(lContext, lBrush, &lRuntime, lImage, bNamed));
    CHECK(lImage.Texture == &lRuntime);
    CHECK(lAssets.LastFrame == -99);   // nobody is asked

    CHECK(ResolveBrush(lContext, lBrush, nullptr, lImage, bNamed));
    CHECK(lImage.Texture == &lAssets.Atlas);
    CHECK(lImage.UVMin.x == doctest::Approx(0.2f));
    CHECK(lAssets.LastFrame == 2);

    lBrush.Sheet.Path = "";
    CHECK(ResolveBrush(lContext, lBrush, nullptr, lImage, bNamed));
    CHECK(lImage.Texture == &lAssets.Plain);
    CHECK(lImage.SizePx.x == doctest::Approx(32.f));
}

TEST_CASE("UIBrush: HasImage is true for a sheet or a texture, false for a plain colour")
{
    UIBrush lBrush;
    CHECK_FALSE(lBrush.HasImage());

    lBrush.Texture.Path = "UI/Art.png";
    CHECK(lBrush.HasImage());

    CHECK(SheetBrush(0).HasImage());
}

// =============================================================================
// Drawing
// =============================================================================
TEST_CASE("UIBrush: nothing named draws one plain quad in its tint")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };

    UIBrush lBrush;
    lBrush.Tint = { 0.2f, 0.4f, 0.6f, 0.8f };

    TDynArray<UIQuad> lQuads;
    CHECK(DrawBrush(lContext, BRUSH_RECT, lBrush, nullptr, lQuads) == EUIBrushDraw::Drawn);
    REQUIRE(lQuads.size() == 1u);
    CHECK(lQuads[0].Texture == nullptr);
    CHECK(lQuads[0].Color.g == doctest::Approx(0.4f));
    CHECK(lQuads[0].Color.a == doctest::Approx(0.8f));
    CHECK(lQuads[0].Bounds.Size().x == doctest::Approx(200.f));

    // No provider at all (a canvas updated without assets): the same plain quad.
    lQuads.clear();
    CHECK(DrawBrush(UIBuildContext{}, BRUSH_RECT, lBrush, nullptr, lQuads) == EUIBrushDraw::Drawn);
    CHECK(lQuads.size() == 1u);
}

TEST_CASE("UIBrush: named but not ready draws nothing and says so, for a sheet as for a texture")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };
    lAssets.bSheetReady   = false;
    lAssets.bTextureReady = false;

    TDynArray<UIQuad> lQuads;
    CHECK(DrawBrush(lContext, BRUSH_RECT, SheetBrush(1), nullptr, lQuads) == EUIBrushDraw::NotReady);
    CHECK(lQuads.empty());

    UIBrush lTexture;
    lTexture.Texture.Path = "UI/Art.png";
    CHECK(DrawBrush(lContext, BRUSH_RECT, lTexture, nullptr, lQuads) == EUIBrushDraw::NotReady);
    CHECK(lQuads.empty());

    // Ready on a later rebuild: drawn.
    lAssets.bSheetReady = true;
    CHECK(DrawBrush(lContext, BRUSH_RECT, SheetBrush(1), nullptr, lQuads) == EUIBrushDraw::Drawn);
    CHECK(lQuads.size() == 9u);
}

TEST_CASE("UIBrush: the frame's border slices it; the brush's own border and scale win")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };

    TDynArray<UIQuad> lQuads;
    UIBrush lBrush = SheetBrush(0);
    DrawBrush(lContext, BRUSH_RECT, lBrush, nullptr, lQuads);
    REQUIRE(lQuads.size() == 9u);
    CHECK(lQuads[0].Bounds.Size().x == doctest::Approx(16.f));   // the frame's 16 px, at scale 1

    lBrush.Border      = UIMargin{ 10.f, 10.f, 10.f, 10.f };
    lBrush.BorderScale = 0.5f;
    lQuads.clear();
    DrawBrush(lContext, BRUSH_RECT, lBrush, nullptr, lQuads);
    REQUIRE(lQuads.size() == 9u);
    CHECK(lQuads[0].Bounds.Size().x == doctest::Approx(5.f));
}

TEST_CASE("UIBrush: a tiled frame past the tile cap is stretched, and DrawBrush says so")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };
    lAssets.FrameMode = ESliceMode::Tile;

    UIBrush lBrush = SheetBrush(0);
    lBrush.BorderScale = 0.01f;   // tiny tiles over a large rect

    TDynArray<UIQuad> lQuads;
    CHECK(DrawBrush(lContext, Bounds2D::FromMinMax({ 0.f, 0.f }, { 1000.f, 1000.f }), lBrush, nullptr, lQuads)
          == EUIBrushDraw::Stretched);
    CHECK(lQuads.size() == 9u);
}

// =============================================================================
// The widgets draw their brush
// =============================================================================
TEST_CASE("UIBrush: an image draws the quads of the brush its fields make")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };
    UICanvas       lCanvas{ 1080.f };
    lCanvas.SetTargetSize(1920, 1080);

    UIImage* lImage = static_cast<UIImage*>(lCanvas.Root().AddChild(NewObject<UIImage>()));
    UIRect   lRect;
    lRect.SizeDelta = { 200.f, 100.f };
    lImage->SetRect(lRect);
    lImage->SetSheetFrame(OpaaxString("UI/Buttons.opaaxasset"), 3);
    lImage->SetColor({ 0.5f, 0.6f, 0.7f, 0.9f });
    lImage->SetBorderScale(0.5f);
    lCanvas.Update(lContext);

    UIBrush lBrush = SheetBrush(3);
    lBrush.Tint        = { 0.5f, 0.6f, 0.7f, 0.9f };
    lBrush.BorderScale = 0.5f;

    TDynArray<UIQuad> lExpected;
    DrawBrush(lContext, lImage->GetBounds(), lBrush, nullptr, lExpected);
    CHECK(lExpected.size() == 9u);
    CheckSameQuads(lImage->GetQuads(), lExpected);
}

TEST_CASE("UIBrush: each button state's brush has its colour and its frame (-1 shows Frame)")
{
    auto lButton = NewObject<UIButton>();
    lButton->Sheet.Path    = "UI/Buttons.opaaxasset";
    lButton->Frame         = 4;
    lButton->HoveredFrame  = 1;
    lButton->PressedFrame  = -1;
    lButton->DisabledFrame = 3;
    lButton->Border        = UIMargin{ 1.f, 2.f, 3.f, 4.f };
    lButton->BorderScale   = 0.25f;

    const UIBrush lHovered = lButton->GetStateBrush(EUIButtonState::Hovered);
    CHECK(lHovered.Frame == 1);
    CHECK(lHovered.Tint.r == doctest::Approx(lButton->Hovered.r));
    CHECK(lHovered.Sheet.Path == lButton->Sheet.Path);
    CHECK(lHovered.Border.Top == doctest::Approx(4.f));
    CHECK(lHovered.BorderScale == doctest::Approx(0.25f));

    CHECK(lButton->GetStateBrush(EUIButtonState::Normal).Frame == 4);
    CHECK(lButton->GetStateBrush(EUIButtonState::Pressed).Frame == 4);
    CHECK(lButton->GetStateBrush(EUIButtonState::Pressed).Tint.r == doctest::Approx(lButton->Pressed.r));
    CHECK(lButton->GetStateBrush(EUIButtonState::Disabled).Frame == 3);
    CHECK(lButton->GetStateBrush(EUIButtonState::Disabled).Tint.a == doctest::Approx(lButton->Disabled.a));
}

TEST_CASE("UIBrush: a button draws its state's brush, Normal then Hovered")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };
    UICanvas       lCanvas{ 1080.f };
    lCanvas.SetTargetSize(1920, 1080);

    UIButton* lButton = static_cast<UIButton*>(lCanvas.Root().AddChild(NewObject<UIButton>()));
    UIRect    lRect;
    lRect.SizeDelta = { 200.f, 100.f };
    lButton->SetRect(lRect);
    lButton->Sheet.Path   = "UI/Buttons.opaaxasset";
    lButton->Frame        = 0;
    lButton->HoveredFrame = 2;
    lButton->InvalidateContent();
    lCanvas.Update(lContext);

    TDynArray<UIQuad> lExpected;
    DrawBrush(lContext, lButton->GetBounds(), lButton->GetStateBrush(EUIButtonState::Normal), nullptr, lExpected);
    CheckSameQuads(lButton->GetQuads(), lExpected);

    lCanvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None });
    lCanvas.Update(lContext);
    REQUIRE(lButton->GetState() == EUIButtonState::Hovered);

    lExpected.clear();
    DrawBrush(lContext, lButton->GetBounds(), lButton->GetStateBrush(EUIButtonState::Hovered), nullptr, lExpected);
    CHECK(lExpected[0].UVMin.x == doctest::Approx(0.2f));   // frame 2
    CheckSameQuads(lButton->GetQuads(), lExpected);
}
