// Suite: following a button (UIWidget::StateSource) — an icon or a label shows the nearest button's
// state, through any widgets between, stopping at a nested button; its own pointer state does not
// count; its modifiers hear each change once; an image with a style swaps its art with the button.
#include <doctest.h>

#include "Core/String/CString.h"
#include "Object/ObjectClassRegistry.h"
#include "RHI/Texture.h"
#include "UI/Modifiers/UITransitionModifier.h"
#include "UI/Styles/UIButtonStyle.h"
#include "UI/UICanvas.h"
#include "UI/UICanvasFile.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    /** Counts the state changes its widget tells it. */
    class CountingModifier final : public UIModifier
    {
        OPAAX_OBJECT(CountingModifier, UIModifier)

    public:
        Uint32              Changes = 0;
        EUIInteractionState Last    = EUIInteractionState::Normal;

    protected:
        void OnStateChanged(EUIInteractionState, const EUIInteractionState InTo) override
        {
            ++Changes;
            Last = InTo;
        }
    };

    /** Never sampled: only its identity matters. */
    class FakeTexture final : public ITexture2D
    {
    public:
        void   Bind(Uint32) const override {}
        void   Unbind()     const override {}
        Uint32 GetWidth()      const noexcept override { return 64u; }
        Uint32 GetHeight()     const noexcept override { return 64u; }
        Uint32 GetRendererID() const noexcept override { return 11u; }
        bool   IsLoaded()      const noexcept override { return true; }
    };

    constexpr const char* STYLE_PATH = "UI/Styles/Icon.opaaxasset";
    constexpr const char* SHEET_PATH = "UI/Icons.opaaxasset";

    /** An icon style: frame 0 in Normal, 1 in Hovered, 2 in Pressed. */
    class StubAssets final : public IUIAssetProvider
    {
    public:
        FakeTexture   Atlas;
        UIButtonStyle Style;

        StubAssets()
        {
            UIBrush* const lStates[] = { &Style.Brushes.Normal, &Style.Brushes.Hovered, &Style.Brushes.Pressed };
            for (Int32 lIndex = 0; lIndex < 3; ++lIndex)
            {
                lStates[lIndex]->Sheet.Path = SHEET_PATH;
                lStates[lIndex]->Frame      = lIndex;
            }
            Style.Brushes.FadeDuration = 0.f;
        }

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return &Atlas; }

        const Asset* ResolveAsset(const char* InAssetPath) override
        {
            return CString::Equals(InAssetPath, STYLE_PATH) ? &Style : nullptr;
        }

        UISheetFrameView ResolveSheetFrame(const char*, Int32) override
        {
            UISheetFrameView lView;
            lView.Texture = &Atlas;
            lView.SizePx  = { 64.f, 64.f };
            return lView;
        }
    };

    /**
     * A 400x100 button at the origin holding a stack, and in it an icon that follows the button and
     * sits outside it (x 500), so the pointer can be over the icon alone.
     */
    struct FollowFixture
    {
        StubAssets     Assets;
        UIBuildContext Context{ &Assets };
        UICanvas       Canvas{ 1080.f };
        UIButton*      Button = nullptr;
        UIWidget*      Stack  = nullptr;
        UIImage*       Icon   = nullptr;

        FollowFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 400.f, 100.f };
            Button->SetRect(lRect);
            Button->Brushes.FadeDuration = 0.f;

            Stack = Button->AddChild(NewObject<UIPanel>());
            Stack->bHitTestable = false;

            Icon = static_cast<UIImage*>(Stack->AddChild(NewObject<UIImage>()));
            UIRect lIconRect;
            lIconRect.SizeDelta        = { 64.f, 64.f };
            lIconRect.AnchoredPosition = { 500.f, 0.f };
            Icon->SetRect(lIconRect);
            Icon->SetStateSource(EUIStateSource::Button);
            Canvas.Update(Context);
        }

        void Move(const Vector2F& InAt)
        {
            Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, InAt });
            Canvas.Update(Context);
        }

        void Press()
        {
            Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Down, { 0.f, 0.f }, EUIPointerButton::Primary });
            Canvas.Update(Context);
        }
    };
}

TEST_CASE("UIStateSource: a follower shows its button's state, through the widgets between")
{
    FollowFixture lF;
    CHECK(&lF.Icon->GetStateSourceWidget() == lF.Button);
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Normal);

    lF.Move({ 0.f, 0.f });
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Hovered);

    lF.Press();
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Pressed);

    lF.Button->SetEnabled(false);
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Disabled);

    // Self (the default): its own state again.
    lF.Icon->SetStateSource(EUIStateSource::Self);
    CHECK(&lF.Icon->GetStateSourceWidget() == lF.Icon);
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Normal);
}

TEST_CASE("UIStateSource: it follows the nearest button, so a nested button keeps its own followers")
{
    FollowFixture lF;

    // A small button inside the big one, holding a follower of its own.
    UIButton* lInner = static_cast<UIButton*>(lF.Button->AddChild(NewObject<UIButton>()));
    UIRect    lRect;
    lRect.SizeDelta        = { 50.f, 50.f };
    lRect.AnchoredPosition = { 150.f, 0.f };
    lInner->SetRect(lRect);
    UIWidget* lInnerIcon = lInner->AddChild(NewObject<UIImage>());
    lInnerIcon->bHitTestable = false;
    lInnerIcon->SetStateSource(EUIStateSource::Button);
    lF.Canvas.Update(lF.Context);

    CHECK(&lInnerIcon->GetStateSourceWidget() == lInner);

    lF.Move({ -150.f, 0.f });   // over the big button only
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Hovered);
    CHECK(lInnerIcon->GetInteractionState() == EUIInteractionState::Normal);

    lF.Move({ 150.f, 0.f });    // over the small one (and so the big one too)
    CHECK(lInnerIcon->GetInteractionState() == EUIInteractionState::Hovered);
}

TEST_CASE("UIStateSource: a follower's own pointer state does not count, its button's does")
{
    FollowFixture lF;
    lF.Button->SetPreviewState(EUIInteractionState::Normal);   // the button holds Normal whatever happens

    lF.Move({ 500.f, 0.f });    // over the icon itself
    REQUIRE(lF.Canvas.GetHovered() == lF.Icon);
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Normal);

    // The button forced to Pressed: the icon follows that too (the editor's Preview state).
    lF.Button->SetPreviewState(EUIInteractionState::Pressed);
    CHECK(lF.Icon->GetInteractionState() == EUIInteractionState::Pressed);
}

TEST_CASE("UIStateSource: a follower's modifiers hear each change of its button once")
{
    FollowFixture lF;
    CountingModifier& lCount = lF.Icon->AddModifier<CountingModifier>();

    lF.Move({ 0.f, 0.f });
    CHECK(lCount.Changes == 1u);
    CHECK(lCount.Last == EUIInteractionState::Hovered);

    lF.Move({ 10.f, 5.f });     // still over the button: no change
    CHECK(lCount.Changes == 1u);

    lF.Press();
    CHECK(lCount.Changes == 2u);
    CHECK(lCount.Last == EUIInteractionState::Pressed);

    lF.Button->SetEnabled(false);
    CHECK(lCount.Changes == 3u);
    CHECK(lCount.Last == EUIInteractionState::Disabled);
}

TEST_CASE("UIStateSource: a transition on a follower plays on its button's hover")
{
    FollowFixture lF;
    UITransitionModifier& lFeel = lF.Icon->AddModifier<UITransitionModifier>();
    lFeel.Hovered.Scale = { 2.f, 2.f };

    lF.Move({ 0.f, 0.f });
    CHECK(lFeel.GetState() == EUIInteractionState::Hovered);
    for (Int32 lFrame = 0; lFrame < 30; ++lFrame) { lF.Canvas.Tick(1.f / 60.f); lF.Canvas.Update(lF.Context); }
    CHECK(lF.Icon->GetDrawTransform().Decompose().Scale.x == doctest::Approx(2.f));
}

TEST_CASE("UIStateSource: an image with a style swaps its art with its button")
{
    FollowFixture lF;
    lF.Icon->SetStyle(OpaaxString(STYLE_PATH));
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Icon->DescribeBrush() == OpaaxString("UI/Icons.opaaxasset#0"));

    lF.Move({ 0.f, 0.f });
    CHECK(lF.Icon->DescribeBrush() == OpaaxString("UI/Icons.opaaxasset#1"));

    lF.Press();
    CHECK(lF.Icon->DescribeBrush() == OpaaxString("UI/Icons.opaaxasset#2"));

    // No style: its own Brush, whatever the state.
    lF.Icon->SetStyle(OpaaxString());
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Icon->DescribeBrush() == OpaaxString("none"));
}

TEST_CASE("UIStateSource: saved with the widget; an older node reads Self")
{
    ObjectClassRegistry lClasses;
    lClasses.Register<UIPanel>();
    lClasses.Register<UIImage>();

    UICanvasFile::UICanvasDoc lDoc = UICanvasFile::MakeEmpty(1080.f);
    auto lIcon = NewObject<UIImage>();
    lIcon->StateSource = EUIStateSource::Button;
    lIcon->Style.Path  = STYLE_PATH;
    lDoc.Root->AddChild(Move(lIcon));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lClasses, lBack));
    const auto* lRead = static_cast<const UIImage*>(lBack.Root->GetChildren()[0].Get());
    CHECK(lRead->StateSource == EUIStateSource::Button);
    CHECK(lRead->Style.Path == OpaaxString(STYLE_PATH));

    UICanvasFile::UICanvasDoc lOld;
    REQUIRE(UICanvasFile::Deserialize(OpaaxString(R"({"Version":2,"Root":{"Type":"UIPanel","Children":[{"Type":"UIImage"}]}})"),
                                      lClasses, lOld));
    CHECK(lOld.Root->GetChildren()[0]->StateSource == EUIStateSource::Self);
}
