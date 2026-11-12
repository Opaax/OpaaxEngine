// Suite: UIButton (UI/Widgets/UIButton.h) — a click is Down then Up while still inside; the quad
// wears the state's colour; disabled falls through like an image. Each state shows its own sheet
// frame, sliced with that frame's border like an image.
#include <doctest.h>

#include "Object/ObjectClassRegistry.h"
#include "RHI/Texture.h"
#include "UI/UICanvas.h"
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
            Button->Sheet.Path    = "UI/Buttons.opaaxasset";
            Button->Frame         = 0;
            Button->HoveredFrame  = 1;
            Button->PressedFrame  = 2;
            Button->DisabledFrame = 3;
            Button->BorderScale   = 0.4f;
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
    CHECK(lF.QuadColor().a == doctest::Approx(lF.Button->Disabled.a));
}

TEST_CASE("UIButton: the quad's colour follows Normal -> Hovered -> Pressed -> Hovered -> Normal")
{
    Fixture lF;
    lF.Button->Normal  = { 0.1f, 0.f, 0.f, 1.f };
    lF.Button->Hovered = { 0.2f, 0.f, 0.f, 1.f };
    lF.Button->Pressed = { 0.3f, 0.f, 0.f, 1.f };
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
    lImage->SetColor(lF.Button->Normal);
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
    lF.Button->Normal  = { 0.1f, 0.f, 0.f, 1.f };
    lF.Button->Hovered = { 0.2f, 0.f, 0.f, 1.f };
    lF.Button->Pressed = { 0.3f, 0.f, 0.f, 1.f };
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
    CHECK(lF.Quads()[0].Color.a == doctest::Approx(lF.Button->Disabled.a));
}

TEST_CASE("UIButton: a state frame of -1 shows Frame; the button's own border wins over the frame's")
{
    SheetFixture lF;
    lF.Button->HoveredFrame = -1;
    lF.Button->Frame        = 2;
    lF.Button->InvalidateContent();

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None);
    lF.Quads();
    CHECK(lF.Assets.LastFrame == 2);
    CHECK(lF.Button->GetStateFrame(EUIButtonState::Hovered) == 2);
    CHECK(lF.Button->GetStateFrame(EUIButtonState::Pressed) == 2);   // its own, set by the fixture

    lF.Button->Border = UIMargin{ 20.f, 20.f, 20.f, 20.f };
    lF.Button->InvalidateContent();
    CHECK(lF.Quads()[0].Bounds.Size().x == doctest::Approx(20.f * 0.4f));
}

TEST_CASE("UIButton: the frames, border and scale round-trip through the file; an old file reads the defaults")
{
    const WidgetClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = NewObject<UIPanel>();

    auto lButton           = NewObject<UIButton>();
    lButton->Sheet.Path    = "UI/Buttons.opaaxasset";
    lButton->Frame         = 0;
    lButton->HoveredFrame  = 1;
    lButton->PressedFrame  = 2;
    lButton->DisabledFrame = 3;
    lButton->Border        = UIMargin{ 1.f, 2.f, 3.f, 4.f };
    lButton->BorderScale   = 0.4f;
    lDoc.Root->AddChild(Move(lButton));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 1u);
    const auto* lRead = static_cast<const UIButton*>(lBack.Root->GetChildren()[0].Get());
    CHECK(lRead->HoveredFrame == 1);
    CHECK(lRead->PressedFrame == 2);
    CHECK(lRead->DisabledFrame == 3);
    CHECK(lRead->Border.Top == doctest::Approx(4.f));
    CHECK(lRead->BorderScale == doctest::Approx(0.4f));

    UICanvasFile::UICanvasDoc lOld;
    REQUIRE(UICanvasFile::Deserialize(OpaaxString(R"({"Version":1,"Root":{"Type":"UIPanel","Children":[{"Type":"UIButton","Frame":2}]}})"),
                                      lRegistry, lOld));
    const auto* lOldButton = static_cast<const UIButton*>(lOld.Root->GetChildren()[0].Get());
    CHECK(lOldButton->Frame == 2);
    CHECK(lOldButton->HoveredFrame == -1);
    CHECK(lOldButton->PressedFrame == -1);
    CHECK(lOldButton->DisabledFrame == -1);
    CHECK(lOldButton->Border.IsZero());
    CHECK(lOldButton->BorderScale == doctest::Approx(1.f));
}
