// Suite: brushes (UI/UIBrush.h) — what fills a rect: runtime texture, then sheet frame, then texture;
// named but not ready draws nothing; nothing named is a plain tinted quad. UIImage and each state of
// UIButton draw the brush their fields make. A stub provider stands in for the renderer's caches.
#include <doctest.h>

#include "Core/String/CString.h"
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

    constexpr const char* CLIP_PATH  = "Anims/Spin.opaaxasset";
    constexpr const char* SHEET_PATH = "UI/Buttons.opaaxasset";

    /**
     * A sheet whose frame N sits at u = 0.1 * N, 64 px with a 16 px border, and a plain texture. Its
     * frames are named A, B, C, D; the clip at CLIP_PATH plays B, D, A at one step a second.
     */
    class StubAssets final : public IUIAssetProvider
    {
    public:
        FakeTexture Atlas{ 512u };
        FakeTexture Plain{ 32u };
        bool        bSheetReady = true;
        bool        bTextureReady = true;
        Int32       LastFrame = -99;
        Uint32      SheetResolves = 0;
        ESliceMode  FrameMode = ESliceMode::Stretch;

        /** What ResolveAsset answers: a clip and its sheet, by path. */
        AnimationClip Clip;
        SpriteSheet   Sheet;
        bool          bClipLoads  = true;
        bool          bSheetLoads = true;

        StubAssets()
        {
            constexpr const char* FRAMES[] = { "A", "B", "C", "D" };
            for (const char* lName : FRAMES)
            {
                SpriteFrame& lFrame = Sheet.Frames.emplace_back();
                lFrame.Name = OpaaxStringID(lName);
                lFrame.Size = { 64.f, 64.f };
            }

            Clip.Sheet.Path = SHEET_PATH;
            Clip.Fps        = 1.f;
            Clip.PlayMode   = EAnimPlayMode::Loop;
            constexpr const char* STEPS[] = { "B", "D", "A" };
            for (const char* lName : STEPS)
            {
                Clip.Steps.emplace_back().Frame = OpaaxStringID(lName);
            }
        }

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return bTextureReady ? &Plain : nullptr; }

        const Asset* ResolveAsset(const char* InAssetPath) override
        {
            if (CString::Equals(InAssetPath, CLIP_PATH))  { return bClipLoads ? &Clip : nullptr; }
            if (CString::Equals(InAssetPath, SHEET_PATH)) { return bSheetLoads ? &Sheet : nullptr; }
            return nullptr;
        }

        UISheetFrameView ResolveSheetFrame(const char*, const Int32 InFrame) override
        {
            ++SheetResolves;
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

    CHECK(ResolveBrush(lContext, lBrush, &lRuntime, 0.f, lImage, bNamed));
    CHECK(lImage.Texture == &lRuntime);
    CHECK(lAssets.LastFrame == -99);   // nobody is asked

    CHECK(ResolveBrush(lContext, lBrush, nullptr, 0.f, lImage, bNamed));
    CHECK(lImage.Texture == &lAssets.Atlas);
    CHECK(lImage.UVMin.x == doctest::Approx(0.2f));
    CHECK(lAssets.LastFrame == 2);

    lBrush.Sheet.Path = "";
    CHECK(ResolveBrush(lContext, lBrush, nullptr, 0.f, lImage, bNamed));
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
    CHECK(DrawBrush(lContext, BRUSH_RECT, lBrush, nullptr, 0.f, lQuads) == EUIBrushDraw::Drawn);
    REQUIRE(lQuads.size() == 1u);
    CHECK(lQuads[0].Texture == nullptr);
    CHECK(lQuads[0].Color.g == doctest::Approx(0.4f));
    CHECK(lQuads[0].Color.a == doctest::Approx(0.8f));
    CHECK(lQuads[0].Bounds.Size().x == doctest::Approx(200.f));

    // No provider at all (a canvas updated without assets): the same plain quad.
    lQuads.clear();
    CHECK(DrawBrush(UIBuildContext{}, BRUSH_RECT, lBrush, nullptr, 0.f, lQuads) == EUIBrushDraw::Drawn);
    CHECK(lQuads.size() == 1u);
}

TEST_CASE("UIBrush: named but not ready draws nothing and says so, for a sheet as for a texture")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };
    lAssets.bSheetReady   = false;
    lAssets.bTextureReady = false;

    TDynArray<UIQuad> lQuads;
    CHECK(DrawBrush(lContext, BRUSH_RECT, SheetBrush(1), nullptr, 0.f, lQuads) == EUIBrushDraw::NotReady);
    CHECK(lQuads.empty());

    UIBrush lTexture;
    lTexture.Texture.Path = "UI/Art.png";
    CHECK(DrawBrush(lContext, BRUSH_RECT, lTexture, nullptr, 0.f, lQuads) == EUIBrushDraw::NotReady);
    CHECK(lQuads.empty());

    // Ready on a later rebuild: drawn.
    lAssets.bSheetReady = true;
    CHECK(DrawBrush(lContext, BRUSH_RECT, SheetBrush(1), nullptr, 0.f, lQuads) == EUIBrushDraw::Drawn);
    CHECK(lQuads.size() == 9u);
}

TEST_CASE("UIBrush: the frame's border slices it; the brush's own border and scale win")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };

    TDynArray<UIQuad> lQuads;
    UIBrush lBrush = SheetBrush(0);
    DrawBrush(lContext, BRUSH_RECT, lBrush, nullptr, 0.f, lQuads);
    REQUIRE(lQuads.size() == 9u);
    CHECK(lQuads[0].Bounds.Size().x == doctest::Approx(16.f));   // the frame's 16 px, at scale 1

    lBrush.Border      = UIMargin{ 10.f, 10.f, 10.f, 10.f };
    lBrush.BorderScale = 0.5f;
    lQuads.clear();
    DrawBrush(lContext, BRUSH_RECT, lBrush, nullptr, 0.f, lQuads);
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
    CHECK(DrawBrush(lContext, Bounds2D::FromMinMax({ 0.f, 0.f }, { 1000.f, 1000.f }), lBrush, nullptr, 0.f, lQuads)
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
    DrawBrush(lContext, lImage->GetBounds(), lBrush, nullptr, 0.f, lExpected);
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
    DrawBrush(lContext, lButton->GetBounds(), lButton->GetStateBrush(EUIButtonState::Normal), nullptr, 0.f, lExpected);
    CheckSameQuads(lButton->GetQuads(), lExpected);

    lCanvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None });
    lCanvas.Update(lContext);
    REQUIRE(lButton->GetState() == EUIButtonState::Hovered);

    lExpected.clear();
    DrawBrush(lContext, lButton->GetBounds(), lButton->GetStateBrush(EUIButtonState::Hovered), nullptr, 0.f, lExpected);
    CHECK(lExpected[0].UVMin.x == doctest::Approx(0.2f));   // frame 2
    CheckSameQuads(lButton->GetQuads(), lExpected);
}

// =============================================================================
// Animated brushes: a clip's steps
// =============================================================================
namespace
{
    UIBrush ClipBrush()
    {
        UIBrush lBrush = SheetBrush(2);   // its own frame shows only when the clip does not load
        lBrush.Animation.Path = CLIP_PATH;
        return lBrush;
    }

    /** The sheet frame DrawBrush asked for at InSeconds, with what it said of the clip. */
    Int32 FrameAt(StubAssets& InAssets, const UIBrush& InBrush, const float InSeconds, UIBrushClip& OutClip)
    {
        UIBuildContext    lContext{ &InAssets };
        TDynArray<UIQuad> lQuads;
        InAssets.LastFrame = -99;
        DrawBrush(lContext, BRUSH_RECT, InBrush, nullptr, InSeconds, lQuads, &OutClip);
        return InAssets.LastFrame;
    }
}

TEST_CASE("UIBrush: a clip wins, and each step shows its frame found by name in the clip's sheet")
{
    StubAssets  lAssets;
    UIBrushClip lClip;
    const UIBrush lBrush = ClipBrush();

    CHECK(FrameAt(lAssets, lBrush, 0.f, lClip) == 1);    // B
    CHECK(lClip.Clip == &lAssets.Clip);
    CHECK(lClip.Step == 0u);
    CHECK_FALSE(lClip.bFinished);
    CHECK(lClip.Issue == EUIBrushIssue::None);

    CHECK(FrameAt(lAssets, lBrush, 1.5f, lClip) == 3);   // D
    CHECK(lClip.Step == 1u);
    CHECK(FrameAt(lAssets, lBrush, 2.f, lClip) == 0);    // A
    CHECK(FrameAt(lAssets, lBrush, 3.f, lClip) == 1);    // looped to B
    CHECK(lBrush.HasImage());

    CHECK(DescribeBrush(lBrush, false, lClip) == OpaaxString("Anims/Spin.opaaxasset#0"));
}

TEST_CASE("UIBrush: a clip with no sheet shows each step's texture; a step without one is a plain quad")
{
    StubAssets lAssets;
    lAssets.Clip.Sheet.Path = "";
    lAssets.Clip.Steps[0].Texture.Path = "Textures/Spin_0.png";

    UIBuildContext    lContext{ &lAssets };
    TDynArray<UIQuad> lQuads;
    CHECK(DrawBrush(lContext, BRUSH_RECT, ClipBrush(), nullptr, 0.f, lQuads) == EUIBrushDraw::Drawn);
    REQUIRE(lQuads.size() == 1u);
    CHECK(lQuads[0].Texture == &lAssets.Plain);
    CHECK(lAssets.SheetResolves == 0u);   // not the brush's own sheet

    lQuads.clear();
    CHECK(DrawBrush(lContext, BRUSH_RECT, ClipBrush(), nullptr, 1.f, lQuads) == EUIBrushDraw::Drawn);
    REQUIRE(lQuads.size() == 1u);
    CHECK(lQuads[0].Texture == nullptr);
}

TEST_CASE("UIBrush: a frame name the sheet lacks shows its default frame, and says which name")
{
    StubAssets lAssets;
    lAssets.Clip.Steps[0].Frame = OpaaxStringID("Nope");

    UIBrushClip lClip;
    CHECK(FrameAt(lAssets, ClipBrush(), 0.f, lClip) == -1);   // the sheet's default
    CHECK(lClip.Issue == EUIBrushIssue::FrameUnknown);
    CHECK(DescribeBrushIssue(ClipBrush(), lClip).Find("'Nope'") >= 0);

    // The clip's sheet not loading: no name resolves, the default shows.
    lAssets.Clip.Steps[0].Frame = OpaaxStringID("B");
    lAssets.bSheetLoads = false;
    CHECK(FrameAt(lAssets, ClipBrush(), 0.f, lClip) == -1);
    CHECK(lClip.Issue == EUIBrushIssue::SheetMissing);
}

TEST_CASE("UIBrush: a clip that does not load leaves the brush's own sheet frame, and says so")
{
    StubAssets lAssets;
    lAssets.bClipLoads = false;

    UIBrushClip lClip;
    CHECK(FrameAt(lAssets, ClipBrush(), 0.f, lClip) == 2);   // the brush's Frame
    CHECK(lClip.Clip == nullptr);
    CHECK(lClip.Issue == EUIBrushIssue::ClipMissing);
    CHECK_FALSE(DescribeBrushIssue(ClipBrush(), lClip).IsEmpty());
    CHECK(DescribeBrush(ClipBrush(), false, lClip) == OpaaxString("UI/Buttons.opaaxasset#2"));
}

TEST_CASE("UIBrush: a Once clip holds its last step and is finished there")
{
    StubAssets lAssets;
    lAssets.Clip.PlayMode = EAnimPlayMode::Once;

    UIBrushClip lClip;
    CHECK(FrameAt(lAssets, ClipBrush(), 2.5f, lClip) == 0);   // A, the last
    CHECK_FALSE(lClip.bFinished);
    CHECK(FrameAt(lAssets, ClipBrush(), 30.f, lClip) == 0);
    CHECK(lClip.Step == 2u);
    CHECK(lClip.bFinished);
}

TEST_CASE("UIBrush: a texture handed over by code beats the clip; DescribeBrush names each source")
{
    StubAssets     lAssets;
    UIBuildContext lContext{ &lAssets };
    FakeTexture    lRuntime{ 4u };

    UIResolvedImage lImage;
    bool            bNamed = false;
    UIBrushClip     lClip;
    CHECK(ResolveBrush(lContext, ClipBrush(), &lRuntime, 0.f, lImage, bNamed, &lClip));
    CHECK(lImage.Texture == &lRuntime);
    CHECK(lClip.Clip == nullptr);
    CHECK(DescribeBrush(ClipBrush(), true, lClip) == OpaaxString("runtime"));

    UIBrush lTexture;
    lTexture.Texture.Path = "UI/Art.png";
    CHECK(DescribeBrush(lTexture, false, UIBrushClip{}) == OpaaxString("UI/Art.png"));
    CHECK(DescribeBrush(UIBrush{}, false, UIBrushClip{}) == OpaaxString("none"));
}

// =============================================================================
// An image plays its clip
// =============================================================================
namespace
{
    struct ClipImageFixture
    {
        StubAssets     Assets;
        UIBuildContext Context{ &Assets };
        UICanvas       Canvas{ 1080.f };
        UIImage*       Image = nullptr;

        ClipImageFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Image = static_cast<UIImage*>(Canvas.Root().AddChild(NewObject<UIImage>()));
            UIRect lRect;
            lRect.SizeDelta = { 64.f, 64.f };
            Image->SetRect(lRect);
            Image->SetAnimation(OpaaxString(CLIP_PATH));
            Canvas.Update(Context);
        }

        /** One frame: the clock, then the walk (as a game does). */
        void Frame(const float InSeconds)
        {
            Canvas.Tick(InSeconds);
            Canvas.Update(Context);
        }
    };
}

TEST_CASE("UIImage: a clip plays from its first step, rebuilt only when the step changes")
{
    ClipImageFixture lF;
    CHECK(lF.Assets.LastFrame == 1);
    CHECK(lF.Image->IsAwake());
    CHECK(lF.Image->DescribeBrush() == OpaaxString("Anims/Spin.opaaxasset#0"));

    const Uint32 lResolves = lF.Assets.SheetResolves;
    lF.Frame(0.25f);
    lF.Frame(0.25f);
    CHECK(lF.Assets.SheetResolves == lResolves);   // still step 0: not rebuilt

    lF.Frame(0.5f);                                 // 1 s: step 1
    CHECK(lF.Assets.SheetResolves == lResolves + 1u);
    CHECK(lF.Assets.LastFrame == 3);
    CHECK(lF.Image->DescribeBrush() == OpaaxString("Anims/Spin.opaaxasset#1"));

    // A loop never sleeps.
    for (Int32 lIndex = 0; lIndex < 10; ++lIndex) { lF.Frame(0.5f); }
    CHECK(lF.Image->IsAwake());
}

TEST_CASE("UIImage: a Once clip sleeps on its last step; ResetRuntime starts it over")
{
    ClipImageFixture lF;
    lF.Assets.Clip.PlayMode = EAnimPlayMode::Once;

    for (Int32 lIndex = 0; lIndex < 8; ++lIndex) { lF.Frame(0.5f); }
    CHECK(lF.Image->DescribeBrush() == OpaaxString("Anims/Spin.opaaxasset#2"));
    CHECK_FALSE(lF.Image->IsAwake());
    CHECK(lF.Canvas.GetAwakeWidgetCount() == 0u);

    const Uint32 lResolves = lF.Assets.SheetResolves;
    lF.Frame(0.5f);
    CHECK(lF.Assets.SheetResolves == lResolves);   // asleep: nothing rebuilt

    lF.Canvas.ResetRuntime();
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Image->DescribeBrush() == OpaaxString("Anims/Spin.opaaxasset#0"));
    CHECK(lF.Image->IsAwake());
}

TEST_CASE("UIImage: on a canvas nobody ticks (the editor's document), a clip shows its first step")
{
    ClipImageFixture lF;
    for (Int32 lIndex = 0; lIndex < 5; ++lIndex) { lF.Canvas.Update(lF.Context); }
    CHECK(lF.Assets.LastFrame == 1);
    CHECK(lF.Image->DescribeBrush() == OpaaxString("Anims/Spin.opaaxasset#0"));
}
