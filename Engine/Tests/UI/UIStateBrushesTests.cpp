// Suite: state brushes (UI/UIStateBrushes.h) and the button drawing them — a state with no image shows
// Normal's in its own tint; the tint fades from the one on screen, an interrupted fade included; a
// state's clip starts over when the state is entered; Focused has its brush; nobody ticking jumps.
#include <doctest.h>

#include "Core/String/CString.h"
#include "RHI/Texture.h"
#include "UI/UICanvas.h"
#include "UI/UIStateBrushes.h"
#include "UI/Widgets/UIButton.h"

using namespace Opaax;

namespace
{
    /** Never sampled: only its identity matters. */
    class FakeTexture final : public ITexture2D
    {
    public:
        void   Bind(Uint32) const override {}
        void   Unbind()     const override {}
        Uint32 GetWidth()      const noexcept override { return 64u; }
        Uint32 GetHeight()     const noexcept override { return 64u; }
        Uint32 GetRendererID() const noexcept override { return 9u; }
        bool   IsLoaded()      const noexcept override { return true; }
    };

    constexpr const char* CLIP_PATH  = "Anims/Glow.opaaxasset";
    constexpr const char* SHEET_PATH = "UI/Buttons.opaaxasset";

    /** A sheet with frames A, B, C, D (frame N at u = 0.1 * N), and a clip playing B, D, A at one step a second. */
    class StubAssets final : public IUIAssetProvider
    {
    public:
        FakeTexture   Atlas;
        AnimationClip Clip;
        SpriteSheet   Sheet;
        Int32         LastFrame = -99;

        StubAssets()
        {
            constexpr const char* FRAMES[] = { "A", "B", "C", "D" };
            for (const char* lName : FRAMES)
            {
                Sheet.Frames.emplace_back().Name = OpaaxStringID(lName);
            }

            Clip.Sheet.Path = SHEET_PATH;
            Clip.Fps        = 1.f;
            constexpr const char* STEPS[] = { "B", "D", "A" };
            for (const char* lName : STEPS)
            {
                Clip.Steps.emplace_back().Frame = OpaaxStringID(lName);
            }
        }

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return &Atlas; }

        const Asset* ResolveAsset(const char* InAssetPath) override
        {
            if (CString::Equals(InAssetPath, CLIP_PATH))  { return &Clip; }
            if (CString::Equals(InAssetPath, SHEET_PATH)) { return &Sheet; }
            return nullptr;
        }

        UISheetFrameView ResolveSheetFrame(const char*, const Int32 InFrame) override
        {
            LastFrame = InFrame;
            UISheetFrameView lView;
            lView.Texture = &Atlas;
            lView.UVMin   = { 0.1f * static_cast<float>(InFrame < 0 ? 0 : InFrame), 0.f };
            lView.UVMax   = { lView.UVMin.x + 0.05f, 1.f };
            lView.SizePx  = { 64.f, 64.f };
            return lView;
        }
    };

    /** A 200x80 button at the origin: black in Normal, red in Hovered, fading linearly over a second. */
    struct ButtonFixture
    {
        StubAssets     Assets;
        UIBuildContext Context{ &Assets };
        UICanvas       Canvas{ 1080.f };
        UIButton*      Button = nullptr;

        ButtonFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 80.f };
            Button->SetRect(lRect);
            Button->Brushes.Normal.Tint   = { 0.f, 0.f, 0.f, 1.f };
            Button->Brushes.Hovered.Tint  = { 1.f, 0.f, 0.f, 1.f };
            Button->Brushes.FadeDuration  = 1.f;
            Button->Brushes.FadeEase      = EEase::Linear;
            Canvas.Update(Context);
        }

        void Hover()   { Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 0.f, 0.f } }); Canvas.Update(Context); }
        void Unhover() { Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 900.f, 500.f } }); Canvas.Update(Context); }

        void Frame(const float InSeconds)
        {
            Canvas.Tick(InSeconds);
            Canvas.Update(Context);
        }

        float Red() const { return Button->GetQuads()[0].Color.x; }
    };
}

// =============================================================================
// The brushes
// =============================================================================
TEST_CASE("UIStateBrushes: a state with no image shows Normal's image and border, in its own tint")
{
    UIStateBrushes lBrushes;
    lBrushes.Normal.Sheet.Path   = SHEET_PATH;
    lBrushes.Normal.Frame        = 3;
    lBrushes.Normal.Border       = UIMargin{ 5.f, 5.f, 5.f, 5.f };
    lBrushes.Normal.BorderScale  = 0.5f;
    lBrushes.Hovered.Tint        = { 0.9f, 0.1f, 0.1f, 1.f };

    const UIBrush lHovered = lBrushes.For(EUIInteractionState::Hovered);
    CHECK(lHovered.Sheet.Path == OpaaxString(SHEET_PATH));
    CHECK(lHovered.Frame == 3);
    CHECK(lHovered.Border.Left == doctest::Approx(5.f));
    CHECK(lHovered.BorderScale == doctest::Approx(0.5f));
    CHECK(lHovered.Tint.x == doctest::Approx(0.9f));

    // A state with its own image keeps it all.
    lBrushes.Pressed.Texture.Path = "UI/Pressed.png";
    const UIBrush lPressed = lBrushes.For(EUIInteractionState::Pressed);
    CHECK(lPressed.Texture.Path == OpaaxString("UI/Pressed.png"));
    CHECK(lPressed.Sheet.IsEmpty());
    CHECK(lPressed.BorderScale == doctest::Approx(1.f));

    // A clip alone counts as an image.
    lBrushes.Focused.Animation.Path = CLIP_PATH;
    CHECK(lBrushes.For(EUIInteractionState::Focused).Sheet.IsEmpty());

    CHECK(&lBrushes.Get(EUIInteractionState::Disabled) == &lBrushes.Disabled);
}

TEST_CASE("UIStateBrushes: FadeTint goes from one tint to the other over the duration, and 0 jumps")
{
    const LinearColor lFrom{ 0.f, 0.f, 0.f, 1.f };
    const LinearColor lTo{ 1.f, 0.5f, 0.f, 0.f };

    CHECK(FadeTint(lFrom, lTo, 0.f, 1.f, EEase::Linear).x == doctest::Approx(0.f));
    const LinearColor lHalf = FadeTint(lFrom, lTo, 0.5f, 1.f, EEase::Linear);
    CHECK(lHalf.x == doctest::Approx(0.5f));
    CHECK(lHalf.y == doctest::Approx(0.25f));
    CHECK(lHalf.w == doctest::Approx(0.5f));
    CHECK(FadeTint(lFrom, lTo, 2.f, 1.f, EEase::Linear).x == doctest::Approx(1.f));
    CHECK(FadeTint(lFrom, lTo, 0.f, 0.f, EEase::Linear).x == doctest::Approx(1.f));

    UIStateBrushes lBrushes;
    lBrushes.FadeDuration = -3.f;
    lBrushes.Sanitize();
    CHECK(lBrushes.FadeDuration == doctest::Approx(0.f));
}

// =============================================================================
// The button drawing them
// =============================================================================
TEST_CASE("UIButton: the tint fades to the new state's, then the button sleeps")
{
    ButtonFixture lF;
    CHECK(lF.Red() == doctest::Approx(0.f));

    lF.Hover();
    CHECK(lF.Red() == doctest::Approx(0.f));   // the fade starts from the tint shown
    CHECK(lF.Button->IsAwake());

    lF.Frame(0.5f);
    CHECK(lF.Red() == doctest::Approx(0.5f));
    CHECK(lF.Button->GetShownTint().x == doctest::Approx(0.5f));

    lF.Frame(0.6f);
    CHECK(lF.Red() == doctest::Approx(1.f));   // exactly the state's at the end
    CHECK_FALSE(lF.Button->IsAwake());
}

TEST_CASE("UIButton: an interrupted fade starts again from the tint on screen")
{
    ButtonFixture lF;
    lF.Hover();
    lF.Frame(0.5f);
    REQUIRE(lF.Red() == doctest::Approx(0.5f));

    lF.Unhover();
    CHECK(lF.Red() == doctest::Approx(0.5f));   // no jump back to red or to black

    lF.Frame(0.5f);
    CHECK(lF.Red() == doctest::Approx(0.25f));
    lF.Frame(0.6f);
    CHECK(lF.Red() == doctest::Approx(0.f));
}

TEST_CASE("UIButton: the first draw and a canvas nobody ticks show the state's tint at once")
{
    ButtonFixture lF;
    lF.Canvas.SetTicked(false);   // the editor's document without Play
    lF.Hover();
    CHECK(lF.Red() == doctest::Approx(1.f));

    // A button disabled before it is first drawn shows Disabled's tint straight away.
    UIButton* lOther = static_cast<UIButton*>(lF.Canvas.Root().AddChild(NewObject<UIButton>()));
    lOther->Brushes.FadeDuration = 1.f;
    lOther->SetEnabled(false);
    lF.Canvas.SetTicked(true);
    lF.Canvas.Update(lF.Context);
    CHECK(lOther->GetQuads()[0].Color.w == doctest::Approx(lOther->Brushes.Disabled.Tint.w));
}

TEST_CASE("UIButton: the image swaps at once, and a state's clip starts over each time it is entered")
{
    ButtonFixture lF;
    lF.Button->Brushes.Normal.Sheet.Path     = SHEET_PATH;
    lF.Button->Brushes.Normal.Frame          = 2;
    lF.Button->Brushes.Hovered.Animation.Path = CLIP_PATH;
    lF.Button->InvalidateContent();
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Assets.LastFrame == 2);

    lF.Hover();
    CHECK(lF.Assets.LastFrame == 1);   // step 0 (B), at once though the tint still fades
    CHECK(lF.Button->DescribeBrush() == OpaaxString("Anims/Glow.opaaxasset#0"));

    lF.Frame(1.5f);
    CHECK(lF.Assets.LastFrame == 3);   // step 1 (D)
    CHECK(lF.Button->IsAwake());       // a looping clip keeps it ticking

    lF.Unhover();
    CHECK(lF.Assets.LastFrame == 2);
    lF.Hover();
    CHECK(lF.Assets.LastFrame == 1);   // from the first step again
}

TEST_CASE("UIButton: focused, it draws the Focused brush")
{
    ButtonFixture lF;
    lF.Button->Brushes.Focused.Tint = { 0.f, 0.f, 1.f, 1.f };
    lF.Button->Brushes.FadeDuration = 0.f;

    lF.Canvas.SetFocus(lF.Button);
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Focused);
    CHECK(lF.Button->GetQuads()[0].Color.z == doctest::Approx(1.f));
}

// =============================================================================
// Preview state (the editor's)
// =============================================================================
TEST_CASE("UIButton: a preview state wins over the pointer; empty gives it back; ResetRuntime clears it")
{
    ButtonFixture lF;
    lF.Button->Brushes.Pressed.Tint = { 0.f, 1.f, 0.f, 1.f };
    lF.Button->Brushes.FadeDuration = 0.f;

    lF.Button->SetPreviewState(EUIInteractionState::Pressed);
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Pressed);
    CHECK(lF.Button->GetQuads()[0].Color.y == doctest::Approx(1.f));   // without any pointer event

    lF.Hover();   // the pointer does not change it
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Pressed);

    lF.Button->SetPreviewState(NullOpt);
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Hovered);   // the pointer's again

    // Disabled shows even on an enabled button, and Stop (ResetRuntime) takes it away.
    lF.Button->SetPreviewState(EUIInteractionState::Disabled);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Disabled);
    CHECK(lF.Button->IsEnabled());
    lF.Canvas.ResetRuntime();
    CHECK_FALSE(lF.Button->GetPreviewState().has_value());
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Normal);
}
