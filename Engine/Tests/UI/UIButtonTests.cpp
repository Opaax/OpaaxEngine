// Suite: UIButton (UI/Widgets/UIButton.h) — a click is Down then Up while still inside; the quad
// wears the state's colour; disabled falls through like an image. Each state shows its own sheet
// frame, sliced with that frame's border like an image. SetEnabled fires its events at once and
// ends a held press.
#include <doctest.h>

#include "Object/ObjectClassRegistry.h"
#include "RHI/Texture.h"
#include "UI/UICanvas.h"
#include "UI/UIAudio.h"
#include "UI/UICanvasFile.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    /** Never sampled: only its identity and size matter. */
    class FakeTexture final : public ITexture2D
    {
    public:
        void   Bind(Uint32) const override {}
        void   Unbind()     const override {}
        Uint32 GetWidth()      const noexcept override { return 512u; }
        Uint32 GetHeight()     const noexcept override { return 512u; }
        Uint32 GetRendererID() const noexcept override { return 7u; }
        bool   IsLoaded()      const noexcept override { return true; }
    };

    /**
     * A sheet of four 256 px frames, each with its own UVs (UVMin.x = 0.1 * index) and border. -1
     * (the sheet's default) answers frame 0, as SpriteSheet::FrameAt does.
     */
    class SheetAssets final : public IUIAssetProvider
    {
    public:
        FakeTexture Atlas;
        Margin      Borders[4] = { Margin{ 112.f, 112.f, 85.f, 112.f }, Margin{ 9.f, 9.f, 9.f, 9.f },
                                   Margin{ 85.f, 85.f, 85.f, 85.f }, Margin{} };
        Int32       LastFrame  = -99;

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return &Atlas; }

        UISheetFrameView ResolveSheetFrame(const char*, const Int32 InFrame) override
        {
            LastFrame          = InFrame;
            const Int32 lIndex = InFrame < 0 ? 0 : InFrame;

            UISheetFrameView lView;
            lView.Texture = &Atlas;
            lView.UVMin   = { 0.1f * static_cast<float>(lIndex), 0.f };
            lView.UVMax   = { 0.1f * static_cast<float>(lIndex) + 0.05f, 0.5f };
            lView.SizePx  = { 256.f, 256.f };
            lView.Border  = Borders[lIndex & 3];
            return lView;
        }
    };

    /** A 420x90 button showing frames 0 to 3 of the sheet, one per state. */
    struct SheetFixture
    {
        SheetAssets    Assets;
        UIBuildContext Context{ &Assets };
        UICanvas       Canvas{ 1080.f };
        UIButton*      Button = nullptr;

        SheetFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 420.f, 90.f };
            Button->SetRect(lRect);
            // Each state its own frame of one sheet, in its own tint; the tint jumps (no fade).
            UIBrush* const lStates[] = { &Button->Brushes.Normal, &Button->Brushes.Hovered,
                                         &Button->Brushes.Pressed, &Button->Brushes.Disabled };
            for (Int32 lIndex = 0; lIndex < 4; ++lIndex)
            {
                lStates[lIndex]->Sheet.Path  = "UI/Buttons.opaaxasset";
                lStates[lIndex]->Frame       = lIndex;
                lStates[lIndex]->BorderScale = 0.4f;
            }
            Button->Brushes.FadeDuration = 0.f;
            Button->InvalidateContent();
        }

        const TDynArray<UIQuad>& Quads()
        {
            Canvas.Update(Context);
            return Button->GetQuads();
        }

        void Route(const EUIPointerEventType InType, const Vector2F& InAt,
                   const EUIPointerButton InButton = EUIPointerButton::Primary)
        {
            Canvas.RoutePointer(UIPointerEvent{ InType, InAt, InButton });
        }
    };

    struct WidgetClasses final : ObjectClassRegistry
    {
        WidgetClasses()
        {
            Register<UIPanel>();
            Register<UIButton>();
        }
    };

    UIPointerEvent Ev(const EUIPointerEventType InType, const Vector2F& InAt,
                      const EUIPointerButton InButton = EUIPointerButton::Primary)
    {
        return UIPointerEvent{ InType, InAt, InButton };
    }

    struct Fixture
    {
        UICanvas  Canvas{ 1080.f };
        UIButton* Button = nullptr;   // 200x80 at the origin
        Uint32    Clicks = 0;

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 80.f };
            Button->SetRect(lRect);
            Button->OnClick.Add([this]() { ++Clicks; });
            Button->Brushes.FadeDuration = 0.f;   // the tint jumps: one Update shows the new state's
            Canvas.Update();
        }

        Vector4F QuadColor()
        {
            Canvas.Update();
            REQUIRE(Button->GetQuads().size() == 1u);
            return Button->GetQuads()[0].Color;
        }
    };
}

TEST_CASE("UIButton: Down then Up inside clicks once, and both are handled")
{
    Fixture lF;

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f })) == EUIReply::Handled);
    CHECK(lF.Clicks == 0);   // not yet — a press is not a click
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 10.f, 10.f })) == EUIReply::Handled);
    CHECK(lF.Clicks == 1);
}

TEST_CASE("UIButton: released outside is no click, but the Up is still consumed — it captured the press")
{
    Fixture lF;

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 900.f, 500.f })) == EUIReply::Handled);
    CHECK(lF.Clicks == 0);

    // An Up with no prior Down on it: inside, but nothing was pressed.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 0);
}

TEST_CASE("UIButton: only the primary button clicks; a secondary press falls through")
{
    Fixture lF;

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }, EUIPointerButton::Secondary)) == EUIReply::Unhandled);
    CHECK(lF.Canvas.GetPressed() == nullptr);
}

TEST_CASE("UIButton: disabled falls through and wears the Disabled colour")
{
    Fixture lF;
    lF.Button->SetEnabled(false);

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f })) == EUIReply::Unhandled);
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 0);
    CHECK(lF.QuadColor().a == doctest::Approx(lF.Button->Brushes.Disabled.Tint.a));
}

TEST_CASE("UIButton: the quad's colour follows Normal -> Hovered -> Pressed -> Hovered -> Normal")
{
    Fixture lF;
    lF.Button->Brushes.Normal.Tint  = { 0.1f, 0.f, 0.f, 1.f };
    lF.Button->Brushes.Hovered.Tint = { 0.2f, 0.f, 0.f, 1.f };
    lF.Button->Brushes.Pressed.Tint = { 0.3f, 0.f, 0.f, 1.f };
    lF.Button->InvalidateContent();

    CHECK(lF.QuadColor().r == doctest::Approx(0.1f));

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));   // Enter
    CHECK(lF.QuadColor().r == doctest::Approx(0.2f));

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    CHECK(lF.QuadColor().r == doctest::Approx(0.3f));

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.QuadColor().r == doctest::Approx(0.2f));   // still hovered

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 900.f, 500.f }, EUIPointerButton::None));   // Leave
    CHECK(lF.QuadColor().r == doctest::Approx(0.1f));
}

// =============================================================================
// Art: a frame per state, sliced like an image
// =============================================================================
TEST_CASE("UIButton: GetState follows Normal -> Hovered -> Pressed, and Disabled wins")
{
    SheetFixture lF;
    lF.Quads();
    CHECK(lF.Button->GetState() == EUIButtonState::Normal);

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None);
    CHECK(lF.Button->GetState() == EUIButtonState::Hovered);

    lF.Route(EUIPointerEventType::Down, { 0.f, 0.f });
    CHECK(lF.Button->GetState() == EUIButtonState::Pressed);

    lF.Button->SetEnabled(false);
    CHECK(lF.Button->GetState() == EUIButtonState::Disabled);

    CHECK(OpaaxString(ToString(EUIButtonState::Pressed)) == OpaaxString("Pressed"));
}

TEST_CASE("UIButton: draws the same quads as an image showing the same frame")
{
    SheetFixture lF;
    const TDynArray<UIQuad> lButtonQuads = lF.Quads();

    UIImage* lImage = static_cast<UIImage*>(lF.Canvas.Root().AddChild(NewObject<UIImage>()));
    UIRect   lRect;
    lRect.SizeDelta = { 420.f, 90.f };
    lImage->SetRect(lRect);
    lImage->SetSheetFrame(OpaaxString("UI/Buttons.opaaxasset"), 0);
    lImage->SetBorderScale(0.4f);
    lImage->SetColor(lF.Button->Brushes.Normal.Tint);
    lF.Canvas.Update(lF.Context);

    const TDynArray<UIQuad>& lImageQuads = lImage->GetQuads();
    REQUIRE(lButtonQuads.size() == 9u);
    REQUIRE(lImageQuads.size() == lButtonQuads.size());
    for (Uint64 lIndex = 0; lIndex < lButtonQuads.size(); ++lIndex)
    {
        CAPTURE(lIndex);
        CHECK(lButtonQuads[lIndex].Bounds.Min().x == doctest::Approx(lImageQuads[lIndex].Bounds.Min().x));
        CHECK(lButtonQuads[lIndex].Bounds.Size().y == doctest::Approx(lImageQuads[lIndex].Bounds.Size().y));
        CHECK(lButtonQuads[lIndex].UVMin.x == doctest::Approx(lImageQuads[lIndex].UVMin.x));
        CHECK(lButtonQuads[lIndex].UVMax.y == doctest::Approx(lImageQuads[lIndex].UVMax.y));
        CHECK(lButtonQuads[lIndex].Color.r == doctest::Approx(lImageQuads[lIndex].Color.r));
    }

    // At 0.4 the 112 px corner is 44.8 units wide.
    CHECK(lButtonQuads[0].Bounds.Size().x == doctest::Approx(44.8f));
}

TEST_CASE("UIButton: each state shows its own frame, sliced with that frame's border, tinted as before")
{
    SheetFixture lF;
    lF.Button->Brushes.Normal.Tint  = { 0.1f, 0.f, 0.f, 1.f };
    lF.Button->Brushes.Hovered.Tint = { 0.2f, 0.f, 0.f, 1.f };
    lF.Button->Brushes.Pressed.Tint = { 0.3f, 0.f, 0.f, 1.f };
    lF.Button->InvalidateContent();

    CHECK(lF.Quads().size() == 9u);
    CHECK(lF.Assets.LastFrame == 0);
    CHECK(lF.Quads()[0].UVMin.x == doctest::Approx(0.f));
    CHECK(lF.Quads()[0].Color.r == doctest::Approx(0.1f));

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None);
    CHECK(lF.Quads().size() == 9u);
    CHECK(lF.Assets.LastFrame == 1);
    CHECK(lF.Quads()[0].UVMin.x == doctest::Approx(0.1f));
    CHECK(lF.Quads()[0].Bounds.Size().x == doctest::Approx(9.f * 0.4f));   // the Hovered frame's 9 px
    CHECK(lF.Quads()[0].Color.r == doctest::Approx(0.2f));

    lF.Route(EUIPointerEventType::Down, { 0.f, 0.f });
    CHECK(lF.Quads()[0].Bounds.Size().x == doctest::Approx(85.f * 0.4f));
    CHECK(lF.Assets.LastFrame == 2);
    CHECK(lF.Quads()[0].Color.r == doctest::Approx(0.3f));

    // The Disabled frame has no border: one quad over the whole button.
    lF.Button->SetEnabled(false);
    REQUIRE(lF.Quads().size() == 1u);
    CHECK(lF.Assets.LastFrame == 3);
    CHECK(lF.Quads()[0].UVMin.x == doctest::Approx(0.3f));
    CHECK(lF.Quads()[0].Color.a == doctest::Approx(lF.Button->Brushes.Disabled.Tint.a));
}

TEST_CASE("UIButton: a state brush with no image shows Normal's image and border, in its own tint")
{
    SheetFixture lF;
    lF.Button->Brushes.Hovered.Sheet.Path = "";
    lF.Button->Brushes.Hovered.Tint       = { 0.6f, 0.f, 0.f, 1.f };
    lF.Button->Brushes.Normal.Frame       = 2;
    lF.Button->InvalidateContent();

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None);
    CHECK(lF.Quads()[0].Color.r == doctest::Approx(0.6f));
    CHECK(lF.Assets.LastFrame == 2);   // Normal's frame
    CHECK(lF.Button->GetStateBrush(EUIInteractionState::Hovered).Frame == 2);
    CHECK(lF.Button->GetStateBrush(EUIInteractionState::Pressed).Frame == 2);   // its own, set by the fixture

    // Normal's border too: its own border wins over the frame's.
    lF.Button->Brushes.Normal.Border = UIMargin{ 20.f, 20.f, 20.f, 20.f };
    lF.Button->InvalidateContent();
    CHECK(lF.Quads()[0].Bounds.Size().x == doctest::Approx(20.f * 0.4f));
}

TEST_CASE("UIButton: the brushes round-trip through the file; a version 1 button reads into them")
{
    const WidgetClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = NewObject<UIPanel>();

    auto lButton = NewObject<UIButton>();
    lButton->Brushes.Hovered.Sheet.Path  = "UI/Buttons.opaaxasset";
    lButton->Brushes.Hovered.Frame       = 1;
    lButton->Brushes.Pressed.Border      = UIMargin{ 1.f, 2.f, 3.f, 4.f };
    lButton->Brushes.Disabled.BorderScale = 0.4f;
    lButton->Brushes.FadeDuration        = 0.25f;
    lButton->Brushes.FadeEase            = EEase::Linear;
    lDoc.Root->AddChild(Move(lButton));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 1u);
    const auto* lRead = static_cast<const UIButton*>(lBack.Root->GetChildren()[0].Get());
    CHECK(lRead->Brushes.Hovered.Sheet.Path == OpaaxString("UI/Buttons.opaaxasset"));
    CHECK(lRead->Brushes.Hovered.Frame == 1);
    CHECK(lRead->Brushes.Pressed.Border.Top == doctest::Approx(4.f));
    CHECK(lRead->Brushes.Disabled.BorderScale == doctest::Approx(0.4f));
    CHECK(lRead->Brushes.FadeDuration == doctest::Approx(0.25f));
    CHECK(lRead->Brushes.FadeEase == EEase::Linear);

    // Version 1: one frame for all, -1 states showing Frame, default colours, and no fade.
    UICanvasFile::UICanvasDoc lOld;
    REQUIRE(UICanvasFile::Deserialize(OpaaxString(R"({"Version":1,"Root":{"Type":"UIPanel","Children":[{"Type":"UIButton","Frame":2}]}})"),
                                      lRegistry, lOld));
    const auto* lOldButton = static_cast<const UIButton*>(lOld.Root->GetChildren()[0].Get());
    CHECK(lOldButton->Brushes.Normal.Frame == 2);
    CHECK(lOldButton->Brushes.Hovered.Frame == 2);
    CHECK(lOldButton->Brushes.Pressed.Frame == 2);
    CHECK(lOldButton->Brushes.Disabled.Frame == 2);
    CHECK(lOldButton->Brushes.Focused.Frame == 2);
    CHECK(lOldButton->Brushes.Hovered.Tint.x == doctest::Approx(0.35f));
    CHECK(lOldButton->Brushes.Disabled.Tint.w == doctest::Approx(0.5f));
    CHECK(lOldButton->Brushes.Normal.Border.IsZero());
    CHECK(lOldButton->Brushes.Normal.BorderScale == doctest::Approx(1.f));
    CHECK(lOldButton->Brushes.FadeDuration == doctest::Approx(0.f));
}

namespace
{
    /** Counts each press and hover event of a button. */
    struct EventCounts
    {
        Uint32 PressedStart = 0, PressedStop = 0, HoveredStart = 0, HoveredStop = 0;

        explicit EventCounts(UIButton& InButton)
        {
            InButton.OnPressedStart.Add([this]() { ++PressedStart; });
            InButton.OnPressedStop.Add([this]() { ++PressedStop; });
            InButton.OnHoveredStart.Add([this]() { ++HoveredStart; });
            InButton.OnHoveredStop.Add([this]() { ++HoveredStop; });
        }
    };
}

TEST_CASE("UIButton: leaving while pressed cancels the press: it stops once, and coming back to release is no click")
{
    Fixture     lF;
    EventCounts lEvents(*lF.Button);

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));   // Enter
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    CHECK(lEvents.PressedStart == 1);
    CHECK(lF.Button->GetState() == EUIButtonState::Pressed);

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 900.f, 500.f }, EUIPointerButton::Primary));   // Leave
    CHECK(lEvents.PressedStart == 1);   // not a second start
    CHECK(lEvents.PressedStop == 1);
    CHECK(lEvents.HoveredStop == 1);
    CHECK_FALSE(lF.Button->GetState() == EUIButtonState::Pressed);

    // Back over it, still held, then released inside: the press was cancelled, so nothing.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::Primary));   // Enter
    CHECK_FALSE(lF.Button->GetState() == EUIButtonState::Pressed);
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f })) == EUIReply::Handled);
    CHECK(lF.Clicks == 0);
    CHECK(lEvents.PressedStop == 1);

    // The next press clicks as usual.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 1);
    CHECK(lEvents.PressedStart == 2);
    CHECK(lEvents.PressedStop == 2);
}

TEST_CASE("UIButton: a press released outside without a Leave still stops, and does not click")
{
    Fixture     lF;
    EventCounts lEvents(*lF.Button);

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 900.f, 500.f }));
    CHECK(lEvents.PressedStop == 1);
    CHECK(lF.Clicks == 0);
}

// =============================================================================
// Enabling and disabling
// =============================================================================
namespace
{
    /** Counts the enable events, and the order the last disable fired them in. */
    struct EnableCounts
    {
        Uint32 Enabled = 0, Disabled = 0, PressedStop = 0;
        Uint32 Fired = 0, DisabledAt = 0, PressedStopAt = 0;

        explicit EnableCounts(UIButton& InButton)
        {
            InButton.OnEnabled.Add([this]() { ++Enabled; });
            InButton.OnDisabled.Add([this]() { ++Disabled; DisabledAt = ++Fired; });
            InButton.OnPressedStop.Add([this]() { ++PressedStop; PressedStopAt = ++Fired; });
        }
    };
}

TEST_CASE("UIButton: SetEnabled fires OnDisabled and OnEnabled at once, once per change, with no pointer event")
{
    Fixture      lF;
    EnableCounts lEvents(*lF.Button);

    lF.Button->SetEnabled(true);   // already enabled: nothing
    CHECK(lEvents.Enabled == 0);

    lF.Button->SetEnabled(false);
    CHECK(lEvents.Disabled == 1);
    CHECK(lEvents.Enabled == 0);
    CHECK(lEvents.PressedStop == 0);   // nothing was pressed
    CHECK(lF.Button->GetState() == EUIButtonState::Disabled);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Disabled);

    lF.Button->SetEnabled(false);
    CHECK(lEvents.Disabled == 1);

    lF.Button->SetEnabled(true);
    CHECK(lEvents.Enabled == 1);
    CHECK(lEvents.Disabled == 1);
    CHECK(lF.Button->GetState() == EUIButtonState::Normal);

    // Pointer events over it afterwards fire neither again.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lEvents.Enabled == 1);
    CHECK(lEvents.Disabled == 1);
    CHECK(lF.Clicks == 1);
}

TEST_CASE("UIButton: a press held when the button is disabled ends there, and its release never clicks")
{
    Fixture      lF;
    EnableCounts lEvents(*lF.Button);

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));   // Enter
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    REQUIRE(lF.Button->GetState() == EUIButtonState::Pressed);
    REQUIRE(lF.Canvas.GetPressed() == lF.Button);

    lF.Button->SetEnabled(false);
    CHECK(lEvents.PressedStop == 1);
    CHECK(lEvents.Disabled == 1);
    CHECK(lEvents.PressedStopAt < lEvents.DisabledAt);   // the press stops first
    CHECK(lF.Canvas.GetPressed() == nullptr);            // the canvas let go of it

    // Released while still disabled: no click, no second stop.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 0);
    CHECK(lEvents.PressedStop == 1);

    // Held again, enabled back mid-hold, then released: the press began while disabled, so nothing.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Button->SetEnabled(true);
    CHECK(lF.Button->GetState() == EUIButtonState::Hovered);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Hovered);
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 0);
    CHECK(lEvents.PressedStop == 1);

    // The next press clicks as usual.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 1);
    CHECK(lEvents.PressedStop == 2);
}

TEST_CASE("UIButton: disabled then enabled back during a press, its release does not click")
{
    Fixture lF;

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Button->SetEnabled(false);
    lF.Button->SetEnabled(true);

    CHECK(lF.Button->GetState() == EUIButtonState::Hovered);
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 0);
}

// =============================================================================
// The Activate stage, and the canvas's audio
// =============================================================================
namespace
{
    /** Counts its owner's activations, and plays a click on the canvas's audio for each. */
    class ActivateCounter final : public UIModifier
    {
        OPAAX_OBJECT(ActivateCounter, UIModifier)

    public:
        Uint32 Activations = 0;

    protected:
        void OnActivated() override
        {
            ++Activations;
            if (IUIAudio* lAudio = GetOwner()->GetCanvas()->GetAudio())
            {
                lAudio->PlaySound(OpaaxString("Audio/Click.wav"), 0.5f);
            }
        }
    };

    /** Remembers what it was asked to play. */
    class StubAudio final : public IUIAudio
    {
    public:
        TDynArray<OpaaxString> Played;
        float                  LastVolume = 0.f;

        void PlaySound(const OpaaxString& InPath, const float InVolume) override
        {
            Played.push_back(InPath);
            LastVolume = InVolume;
        }
    };
}

TEST_CASE("UIButton: a click runs the Activate stage once, before OnClick; the canvas's audio plays it")
{
    Fixture         lF;
    StubAudio       lAudio;
    ActivateCounter& lCounter = lF.Button->AddModifier<ActivateCounter>();
    lF.Canvas.SetAudio(&lAudio);
    CHECK(lF.Canvas.GetAudio() == &lAudio);

    Uint32 lActivationsAtClick = 99;
    lF.Button->OnClick.Add([&]() { lActivationsAtClick = lCounter.Activations; });

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    CHECK(lCounter.Activations == 0u);   // a press is not an action
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lCounter.Activations == 1u);
    CHECK(lActivationsAtClick == 1u);    // the stage ran before OnClick
    REQUIRE(lAudio.Played.size() == 1u);
    CHECK(lAudio.Played[0] == OpaaxString("Audio/Click.wav"));
    CHECK(lAudio.LastVolume == doctest::Approx(0.5f));

    // No audio: silence, and nothing breaks.
    lF.Canvas.SetAudio(nullptr);
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lCounter.Activations == 2u);
    CHECK(lAudio.Played.size() == 1u);
}

TEST_CASE("UIButton: no Activate on a cancelled press, a release outside, a disabled button or a disabled modifier")
{
    Fixture          lF;
    ActivateCounter& lCounter = lF.Button->AddModifier<ActivateCounter>();

    // Pressed, left (the press is cancelled), released back over it.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 900.f, 500.f }, EUIPointerButton::Primary));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::Primary));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lCounter.Activations == 0u);

    // Released outside.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 900.f, 500.f }));
    CHECK(lCounter.Activations == 0u);

    // Disabled: clicks fall through.
    lF.Button->SetEnabled(false);
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lCounter.Activations == 0u);

    // Enabled again, but the modifier off: a click, and no stage for it.
    lF.Button->SetEnabled(true);
    lCounter.bEnabled = false;
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lF.Clicks == 1);
    CHECK(lCounter.Activations == 0u);
}

TEST_CASE("UIButton: a widget following the button hears its Activate; a nested button's followers do not")
{
    Fixture lF;
    UIWidget* lLabel = lF.Button->AddChild(NewObject<UIImage>());
    lLabel->bHitTestable = false;
    lLabel->SetStateSource(EUIStateSource::Button);
    ActivateCounter& lLabelCounter = lLabel->AddModifier<ActivateCounter>();

    UIWidget* lInner = lF.Button->AddChild(NewObject<UIButton>());
    lInner->bHitTestable = false;
    UIWidget* lInnerLabel = lInner->AddChild(NewObject<UIImage>());
    lInnerLabel->bHitTestable = false;   // the click lands on the outer button
    lInnerLabel->SetStateSource(EUIStateSource::Button);
    ActivateCounter& lInnerCounter = lInnerLabel->AddModifier<ActivateCounter>();
    lF.Canvas.Update();

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f }));
    CHECK(lLabelCounter.Activations == 1u);
    CHECK(lInnerCounter.Activations == 0u);
}
