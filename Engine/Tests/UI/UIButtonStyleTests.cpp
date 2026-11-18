// Suite: the UI Button Style asset (UI/Styles/UIButtonStyle.h) — its file round trip and fixes; a
// button naming one draws its brushes, and its own when it does not load; a saved style redraws the
// canvas that shows it (the asset provider's revision).
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/String/CString.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"
#include "UI/Styles/UIButtonStyle.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"

using namespace Opaax;

namespace
{
    constexpr const char* STYLE_PATH = "UI/Styles/Pixel.opaaxasset";

    /** Answers the style at STYLE_PATH (when bLoads), and a revision a test bumps as a reload would. */
    class StubAssets final : public IUIAssetProvider
    {
    public:
        UIButtonStyle Style;
        bool          bLoads   = true;
        Uint64        Revision = 0;

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return nullptr; }

        const Asset* ResolveAsset(const char* InAssetPath) override
        {
            return bLoads && CString::Equals(InAssetPath, STYLE_PATH) ? &Style : nullptr;
        }

        Uint64 GetRevision() override { return Revision; }
    };

    /** A 200x80 button at the origin naming the style, which tints Normal green and Hovered blue. */
    struct StyleFixture
    {
        StubAssets     Assets;
        UIBuildContext Context{ &Assets };
        UICanvas       Canvas{ 1080.f };
        UIButton*      Button = nullptr;

        StyleFixture()
        {
            Assets.Style.Brushes.Normal.Tint  = { 0.f, 1.f, 0.f, 1.f };
            Assets.Style.Brushes.Hovered.Tint = { 0.f, 0.f, 1.f, 1.f };
            Assets.Style.Brushes.FadeDuration = 0.f;

            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 80.f };
            Button->SetRect(lRect);
            Button->Brushes.Normal.Tint  = { 1.f, 0.f, 0.f, 1.f };   // its own: red
            Button->Brushes.FadeDuration = 0.f;
            Button->SetStyle(OpaaxString(STYLE_PATH));
            Canvas.Update(Context);
        }

        Vector4F Color() const { return Button->GetQuads()[0].Color; }
    };
}

TEST_CASE("UIButtonStyle asset: save then load round-trips its brushes; the fade stays at 0 or more")
{
    const TestTempDir   lDir("uiButtonStyle");
    ObjectClassRegistry lClasses;
    lClasses.Register<UIButtonStyle>();

    UIButtonStyle lSaved;
    lSaved.Brushes.Hovered.Sheet.Path = "UI/SlicedButtons.opaaxasset";
    lSaved.Brushes.Hovered.Frame      = 1;
    lSaved.Brushes.Pressed.Animation.Path = "Anims/Cycle.opaaxasset";
    lSaved.Brushes.Disabled.Tint      = { 0.5f, 0.5f, 0.5f, 0.25f };
    lSaved.Brushes.FadeDuration       = 0.3f;
    lSaved.Brushes.FadeEase           = EEase::SineInOut;

    const OpaaxString lPath = lDir.Sub("Pixel.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lSaved));

    const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPath, lClasses);
    const UIButtonStyle*          lLoaded = Cast<UIButtonStyle>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);
    CHECK(lLoaded->Brushes.Hovered.Sheet.Path == OpaaxString("UI/SlicedButtons.opaaxasset"));
    CHECK(lLoaded->Brushes.Hovered.Frame == 1);
    CHECK(lLoaded->Brushes.Pressed.Animation.Path == OpaaxString("Anims/Cycle.opaaxasset"));
    CHECK(lLoaded->Brushes.Disabled.Tint.w == doctest::Approx(0.25f));
    CHECK(lLoaded->Brushes.FadeDuration == doctest::Approx(0.3f));
    CHECK(lLoaded->Brushes.FadeEase == EEase::SineInOut);
    CHECK(AssetFile::Serialize(*lLoaded) == AssetFile::Serialize(lSaved));

    UIButtonStyle lEdited;
    lEdited.Brushes.FadeDuration = -1.f;
    lEdited.PostEditChange();
    CHECK(lEdited.Brushes.FadeDuration == 0.f);
}

TEST_CASE("UIButtonStyle: a button naming a style draws the style's brushes, not its own")
{
    StyleFixture lF;
    CHECK(lF.Color().y == doctest::Approx(1.f));   // the style's green, not its own red
    CHECK(&lF.Button->GetBrushes() == &lF.Assets.Style.Brushes);

    lF.Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 0.f, 0.f } });
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Color().z == doctest::Approx(1.f));   // the style's Hovered

    // No style named: its own again.
    lF.Button->SetStyle(OpaaxString());
    lF.Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 900.f, 500.f } });
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Color().x == doctest::Approx(1.f));
}

TEST_CASE("UIButtonStyle: a style that does not load leaves the button's own brushes")
{
    StyleFixture lF;
    lF.Assets.bLoads = false;
    lF.Button->InvalidateContent();
    lF.Canvas.Update(lF.Context);

    CHECK(lF.Color().x == doctest::Approx(1.f));   // its own red
    CHECK(&lF.Button->GetBrushes() == &lF.Button->Brushes);
}

TEST_CASE("UIButtonStyle: a saved style redraws the canvas that shows it, with no pointer event")
{
    StyleFixture lF;
    REQUIRE(lF.Color().y == doctest::Approx(1.f));

    // Edited and reloaded in place, as the editor's save does: nothing tells the button but the revision.
    lF.Assets.Style.Brushes.Normal.Tint = { 1.f, 1.f, 0.f, 1.f };
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Color().x == doctest::Approx(0.f));   // not redrawn yet: nothing changed for the canvas

    ++lF.Assets.Revision;
    lF.Canvas.Update(lF.Context);
    CHECK(lF.Color().x == doctest::Approx(1.f));   // yellow now
}
