// Suite: the sound modifier (UI/Modifiers/UISoundModifier.h) and the UI Sounds asset — hover once per
// coming over, press, click, show and hide, through the canvas's audio; a preset wins; a disabled
// button or modifier is silent; a follower plays on its button's hover and click.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/String/CString.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"
#include "UI/Modifiers/UISoundModifier.h"
#include "UI/UIAudio.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"

using namespace Opaax;

namespace
{
    constexpr const char* PRESET_PATH = "UI/Sounds/Menu.opaaxasset";

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

    /** Answers the sound set at PRESET_PATH. */
    class StubAssets final : public IUIAssetProvider
    {
    public:
        UISoundSet Set;

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }
        ITexture2D*  ResolveTexture(const char*) override { return nullptr; }
        const Asset* ResolveAsset(const char* InAssetPath) override
        {
            return CString::Equals(InAssetPath, PRESET_PATH) ? &Set : nullptr;
        }
    };

    /** A 200x80 button at the origin with a sound modifier of its own sounds, on a canvas with audio. */
    struct SoundFixture
    {
        StubAssets       Assets;
        UIBuildContext   Context{ &Assets };
        StubAudio        Audio;
        UICanvas         Canvas{ 1080.f };
        UIButton*        Button = nullptr;
        UISoundModifier* Sound  = nullptr;

        SoundFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Canvas.SetAudio(&Audio);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 80.f };
            Button->SetRect(lRect);

            Sound = &Button->AddModifier<UISoundModifier>();
            Sound->Sounds.Hover  = "Audio/Hover.wav";
            Sound->Sounds.Press  = "Audio/Press.wav";
            Sound->Sounds.Click  = "Audio/Click.wav";
            Sound->Sounds.Show   = "Audio/Show.wav";
            Sound->Sounds.Hide   = "Audio/Hide.wav";
            Sound->Sounds.Volume = 0.7f;
            Canvas.Update(Context);
            Audio.Played.clear();   // the show it joined with
        }

        void Route(const EUIPointerEventType InType, const Vector2F& InAt,
                   const EUIPointerButton InButton = EUIPointerButton::None)
        {
            Canvas.RoutePointer(UIPointerEvent{ InType, InAt, InButton });
        }

        void Click()
        {
            Route(EUIPointerEventType::Down, { 0.f, 0.f }, EUIPointerButton::Primary);
            Route(EUIPointerEventType::Up, { 0.f, 0.f }, EUIPointerButton::Primary);
        }
    };
}

TEST_CASE("UISoundModifier: hover once per coming over, then press and click, at its volume")
{
    SoundFixture lF;

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    lF.Route(EUIPointerEventType::Move, { 10.f, 5.f });   // still over it: no second hover
    REQUIRE(lF.Audio.Played.size() == 1u);
    CHECK(lF.Audio.Played[0] == OpaaxString("Audio/Hover.wav"));
    CHECK(lF.Audio.LastVolume == doctest::Approx(0.7f));

    lF.Click();   // press, then click; the release back to Hovered is not a hover
    REQUIRE(lF.Audio.Played.size() == 3u);
    CHECK(lF.Audio.Played[1] == OpaaxString("Audio/Press.wav"));
    CHECK(lF.Audio.Played[2] == OpaaxString("Audio/Click.wav"));
    CHECK(lF.Sound->GetLastSound() == OpaaxString("Audio/Click.wav"));
    CHECK(lF.Sound->GetSoundCount() == 3u);

    lF.Route(EUIPointerEventType::Move, { 900.f, 500.f });
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });   // over it again: a new hover
    CHECK(lF.Audio.Played.size() == 4u);
    CHECK(lF.Audio.Played.back() == OpaaxString("Audio/Hover.wav"));
}

TEST_CASE("UISoundModifier: Show and Hide play their sounds; Hide does not wait for them")
{
    SoundFixture lF;

    lF.Button->Hide();
    CHECK_FALSE(lF.Button->bVisible);   // hidden at once
    REQUIRE(lF.Audio.Played.size() == 1u);
    CHECK(lF.Audio.Played[0] == OpaaxString("Audio/Hide.wav"));

    lF.Button->Show();
    REQUIRE(lF.Audio.Played.size() == 2u);
    CHECK(lF.Audio.Played[1] == OpaaxString("Audio/Show.wav"));
}

TEST_CASE("UISoundModifier: a disabled button is silent, and so is a disabled modifier")
{
    SoundFixture lF;
    lF.Button->SetEnabled(false);
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    lF.Click();
    CHECK(lF.Audio.Played.empty());

    lF.Sound->bEnabled = false;   // off first: enabling the button under the pointer is a hover
    lF.Button->SetEnabled(true);
    lF.Route(EUIPointerEventType::Move, { 900.f, 500.f });
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    lF.Click();
    CHECK(lF.Audio.Played.empty());
    CHECK(lF.Sound->GetSoundCount() == 0u);
}

TEST_CASE("UISoundModifier: a preset that loads wins over its own sounds; one that does not leaves them")
{
    SoundFixture lF;
    lF.Assets.Set.Sounds.Hover  = "Audio/PresetHover.wav";
    lF.Assets.Set.Sounds.Volume = 0.3f;
    lF.Sound->SetPreset(OpaaxString(PRESET_PATH));

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    REQUIRE(lF.Audio.Played.size() == 1u);
    CHECK(lF.Audio.Played[0] == OpaaxString("Audio/PresetHover.wav"));
    CHECK(lF.Audio.LastVolume == doctest::Approx(0.3f));

    lF.Sound->SetPreset(OpaaxString("UI/Sounds/Missing.opaaxasset"));
    lF.Route(EUIPointerEventType::Move, { 900.f, 500.f });
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    CHECK(lF.Audio.Played.back() == OpaaxString("Audio/Hover.wav"));
}

TEST_CASE("UISoundModifier: on a widget following a button, it plays on the button's hover and click")
{
    SoundFixture lF;
    lF.Sound->bEnabled = false;   // only the follower's sounds now

    UIWidget* lIcon = lF.Button->AddChild(NewObject<UIImage>());
    lIcon->bHitTestable = false;
    lIcon->SetStateSource(EUIStateSource::Button);
    UISoundModifier& lIconSound = lIcon->AddModifier<UISoundModifier>();
    lIconSound.Sounds.Hover = "Audio/IconHover.wav";
    lIconSound.Sounds.Click = "Audio/IconClick.wav";
    lF.Canvas.Update(lF.Context);
    lF.Audio.Played.clear();

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    lF.Click();
    REQUIRE(lF.Audio.Played.size() == 2u);
    CHECK(lF.Audio.Played[0] == OpaaxString("Audio/IconHover.wav"));
    CHECK(lF.Audio.Played[1] == OpaaxString("Audio/IconClick.wav"));
}

TEST_CASE("UISoundModifier: no audio on the canvas is silence, but it still counts what it asked for")
{
    SoundFixture lF;
    lF.Canvas.SetAudio(nullptr);
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    CHECK(lF.Audio.Played.empty());
    CHECK(lF.Sound->GetSoundCount() == 1u);
    CHECK(lF.Sound->GetLastSound() == OpaaxString("Audio/Hover.wav"));
}

TEST_CASE("UISoundSet asset: save then load round-trips its sounds; Volume stays at 0 or more")
{
    const TestTempDir   lDir("uiSoundSet");
    ObjectClassRegistry lClasses;
    lClasses.Register<UISoundSet>();

    UISoundSet lSaved;
    lSaved.Sounds.Hover  = "Audio/Step0.wav";
    lSaved.Sounds.Click  = "Audio/Beep.wav";
    lSaved.Sounds.Volume = 0.5f;

    const OpaaxString lPath = lDir.Sub("Menu.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lSaved));
    const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPath, lClasses);
    const UISoundSet*             lLoaded = Cast<UISoundSet>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);
    CHECK(lLoaded->Sounds.Hover == OpaaxString("Audio/Step0.wav"));
    CHECK(lLoaded->Sounds.Click == OpaaxString("Audio/Beep.wav"));
    CHECK(lLoaded->Sounds.Volume == doctest::Approx(0.5f));

    UISoundSet lEdited;
    lEdited.Sounds.Volume = -2.f;
    lEdited.PostEditChange();
    CHECK(lEdited.Sounds.Volume == 0.f);
}
