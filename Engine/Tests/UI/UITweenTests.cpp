// Suite: UI tweens (UI/Animation/UITween.h). Sampling is stateless: the pose of a moment, whatever the
// frames before it. Before the first key its pose; between two, eased by the second's curve; past the
// end held or at rest; Loop and PingPong wrap. And the asset: its file round trip, its keys kept sorted.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"
#include "UI/Animation/UITween.h"
#include "UI/Modifiers/UITweenModifier.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    UITweenKey Key(const float InTime, const float InScale, const EEase InEase = EEase::Linear)
    {
        UITweenKey lKey;
        lKey.Time        = InTime;
        lKey.Ease        = InEase;
        lKey.Pose.Scale  = { InScale, InScale };
        return lKey;
    }

    /** 0.5 at 0.1 s, 1 at 0.3 s, linear: a 0.2 s grow after a 0.1 s wait. */
    TDynArray<UITweenKey> Grow()
    {
        return { Key(0.1f, 0.5f), Key(0.3f, 1.f) };
    }

    float ScaleAt(const TDynArray<UITweenKey>& InKeys, const EUILoop InLoop, const bool bInHold, const float InTime,
                  bool& OutFinished)
    {
        return SampleTweenKeys(InKeys, InLoop, bInHold, InTime, OutFinished).Scale.x;
    }

    float ScaleAt(const TDynArray<UITweenKey>& InKeys, const EUILoop InLoop, const float InTime)
    {
        bool lFinished = false;
        return ScaleAt(InKeys, InLoop, true, InTime, lFinished);
    }
}

TEST_CASE("UITween: before the first key its pose, between two eased by the second's curve")
{
    bool lFinished = true;
    CHECK(ScaleAt(Grow(), EUILoop::Once, true, 0.f, lFinished) == doctest::Approx(0.5f));
    CHECK_FALSE(lFinished);
    CHECK(ScaleAt(Grow(), EUILoop::Once, 0.05f) == doctest::Approx(0.5f));

    // Halfway between the keys, linear: halfway between the poses.
    CHECK(ScaleAt(Grow(), EUILoop::Once, 0.2f) == doctest::Approx(0.75f));

    // The second key's curve shapes the motion into it.
    TDynArray<UITweenKey> lEased = Grow();
    lEased[1].Ease = EEase::QuadIn;
    CHECK(ScaleAt(lEased, EUILoop::Once, 0.2f) == doctest::Approx(0.5f + 0.5f * 0.25f));
}

TEST_CASE("UITween: past the end, Once holds the last pose and finishes, or goes to rest without bHoldEnd")
{
    bool lFinished = false;
    CHECK(ScaleAt(Grow(), EUILoop::Once, true, 0.3f, lFinished) == doctest::Approx(1.f));
    CHECK(lFinished);

    TDynArray<UITweenKey> lShrink = { Key(0.f, 1.f), Key(0.2f, 0.f) };
    CHECK(ScaleAt(lShrink, EUILoop::Once, true, 5.f, lFinished) == doctest::Approx(0.f));
    CHECK(lFinished);
    CHECK(ScaleAt(lShrink, EUILoop::Once, false, 5.f, lFinished) == doctest::Approx(1.f));   // the rest pose
    CHECK(lFinished);

    // Just before the end: not finished.
    ScaleAt(lShrink, EUILoop::Once, true, 0.19f, lFinished);
    CHECK_FALSE(lFinished);
}

TEST_CASE("UITween: Loop starts again, PingPong comes back; neither finishes")
{
    const TDynArray<UITweenKey> lKeys = { Key(0.f, 1.f), Key(1.f, 2.f) };
    bool lFinished = true;

    CHECK(ScaleAt(lKeys, EUILoop::Loop, true, 1.25f, lFinished) == doctest::Approx(1.25f));
    CHECK_FALSE(lFinished);
    CHECK(ScaleAt(lKeys, EUILoop::Loop, 3.5f) == doctest::Approx(1.5f));

    CHECK(ScaleAt(lKeys, EUILoop::PingPong, true, 1.25f, lFinished) == doctest::Approx(1.75f));   // on the way back
    CHECK_FALSE(lFinished);
    CHECK(ScaleAt(lKeys, EUILoop::PingPong, 2.f) == doctest::Approx(1.f));
    CHECK(ScaleAt(lKeys, EUILoop::PingPong, 2.25f) == doctest::Approx(1.25f));
}

TEST_CASE("UITween: a hitch skips ahead: the pose of the moment, not of the frames before it")
{
    const TDynArray<UITweenKey> lKeys = { Key(0.f, 0.f), Key(0.1f, 1.f), Key(0.2f, 0.f), Key(0.3f, 1.f) };

    // Straight to 0.25 s, or there after other moments: the same pose, nothing carried between samples.
    const float lStraight = ScaleAt(lKeys, EUILoop::Once, 0.25f);
    ScaleAt(lKeys, EUILoop::Once, 0.05f);
    ScaleAt(lKeys, EUILoop::Once, 0.15f);
    CHECK(lStraight == doctest::Approx(0.5f));
    CHECK(ScaleAt(lKeys, EUILoop::Once, 0.25f) == lStraight);
}

TEST_CASE("UITween: one key holds its pose; no key is the rest pose")
{
    const TDynArray<UITweenKey> lOne = { Key(0.4f, 3.f) };
    bool lFinished = false;
    CHECK(ScaleAt(lOne, EUILoop::Once, true, 0.f, lFinished) == doctest::Approx(3.f));
    CHECK_FALSE(lFinished);
    CHECK(ScaleAt(lOne, EUILoop::Once, true, 1.f, lFinished) == doctest::Approx(3.f));
    CHECK(lFinished);
    CHECK(ScaleAt(lOne, EUILoop::Loop, 7.f) == doctest::Approx(3.f));

    const TDynArray<UITweenKey> lNone;
    CHECK(SampleTweenKeys(lNone, EUILoop::Once, true, 0.5f, lFinished).IsRest());
    CHECK(lFinished);
    CHECK(SampleTweenKeys(lNone, EUILoop::Loop, true, 0.5f, lFinished).IsRest());
    CHECK_FALSE(lFinished);
}

TEST_CASE("UITween asset: save then load round-trips every field; edited keys are sorted by time")
{
    const TestTempDir   lDir("uiTween");
    ObjectClassRegistry lClasses;
    lClasses.Register<UITween>();

    UITween lSaved;
    lSaved.Keys     = { Key(0.f, 0.8f), Key(0.18f, 1.f, EEase::BackOut) };
    lSaved.Keys[0].Pose.Opacity = 0.f;
    lSaved.Loop     = EUILoop::PingPong;
    lSaved.bHoldEnd = false;

    const OpaaxString lPath = lDir.Sub("PopIn.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lSaved));

    const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPath, lClasses);
    const UITween*                lLoaded = Cast<UITween>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);
    REQUIRE(lLoaded->Keys.size() == 2u);
    CHECK(lLoaded->Keys[1].Time == doctest::Approx(0.18f));
    CHECK(lLoaded->Keys[1].Ease == EEase::BackOut);
    CHECK(lLoaded->Keys[0].Pose.Opacity == doctest::Approx(0.f));
    CHECK(lLoaded->Loop == EUILoop::PingPong);
    CHECK_FALSE(lLoaded->bHoldEnd);
    CHECK(lLoaded->GetLength() == doctest::Approx(0.18f));
    CHECK(AssetFile::Serialize(*lLoaded) == AssetFile::Serialize(lSaved));

    AssetFile::Contents lContents;
    REQUIRE(AssetFile::Load(lPath, lContents));
    CHECK(lContents.Properties["Loop"] == "PingPong");

    // Added out of order in the editor: sorted on edit, equal times keep their order.
    UITween lEdited;
    lEdited.Keys = { Key(0.3f, 3.f), Key(0.1f, 1.f), Key(0.2f, 2.f), Key(0.1f, 4.f) };
    lEdited.PostEditChange();
    CHECK(lEdited.Keys[0].Pose.Scale.x == doctest::Approx(1.f));
    CHECK(lEdited.Keys[1].Pose.Scale.x == doctest::Approx(4.f));
    CHECK(lEdited.Keys[2].Pose.Scale.x == doctest::Approx(2.f));
    CHECK(lEdited.Keys[3].Pose.Scale.x == doctest::Approx(3.f));

    bool lFinished = false;
    CHECK(SampleTween(lEdited, 0.25f, lFinished).Scale.x == doctest::Approx(2.5f));
}

// =============================================================================
// The tween modifier
// =============================================================================

namespace
{
    /** A canvas and an image with a tween modifier; frames ticked then drawn. */
    struct TweenFixture
    {
        UICanvas         Canvas{ 1080.f };
        UIWidget*        Image = nullptr;
        UITweenModifier* Tween = nullptr;
        Int32            Finished = 0;

        TweenFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Image = Canvas.Root().AddChild(NewObject<UIImage>());
            UIRect lRect;
            lRect.SizeDelta = { 100.f, 100.f };
            Image->SetRect(lRect);

            Tween = &Image->AddModifier<UITweenModifier>();
            Tween->Keys = Grow();   // 0.5 at 0.1 s, 1 at 0.3 s
            Tween->OnFinished.Add([this]() { ++Finished; });
            Canvas.Update();
        }

        void Run(const float InSeconds, const float InHz = 60.f)
        {
            const Int32 lFrames = static_cast<Int32>(InSeconds * InHz + 0.5f);
            for (Int32 lFrame = 0; lFrame < lFrames; ++lFrame)
            {
                Canvas.Tick(1.f / InHz);
                Canvas.Update();
            }
        }

        float Scale()
        {
            Canvas.Update();
            return Image->GetDrawTransform().Decompose().Scale.x;
        }
    };
}

TEST_CASE("UITweenModifier: Play starts at the first key, at once")
{
    TweenFixture lF;
    CHECK(lF.Scale() == doctest::Approx(1.f));   // not playing: at rest
    CHECK_FALSE(lF.Tween->IsPlaying());

    lF.Tween->Play();
    CHECK(lF.Tween->IsPlaying());
    CHECK(lF.Scale() == doctest::Approx(0.5f));   // before any tick: no flash at full size

    lF.Run(0.2f);
    CHECK(lF.Scale() == doctest::Approx(0.75f).epsilon(1e-3));
}

TEST_CASE("UITweenModifier: Delay holds the first key before it moves")
{
    TweenFixture lF;
    lF.Tween->SetDelay(0.2f);
    lF.Tween->Play();

    lF.Run(0.25f);   // 0.05 s into the keys: still before the first one at 0.1 s
    CHECK(lF.Scale() == doctest::Approx(0.5f));
    lF.Run(0.15f);   // 0.2 s into the keys
    CHECK(lF.Scale() == doctest::Approx(0.75f).epsilon(1e-3));
}

TEST_CASE("UITweenModifier: OnFinished fires once at the end; the last pose holds; Stop goes to rest and sleeps")
{
    TweenFixture lF;
    lF.Tween->Play();
    lF.Run(1.f);
    CHECK(lF.Finished == 1);
    CHECK_FALSE(lF.Tween->IsPlaying());
    CHECK(lF.Canvas.GetAwakeCount() == 0u);
    CHECK(lF.Scale() == doctest::Approx(1.f));   // held: Grow ends at 1

    // Held at a pose that is not the rest pose: Stop puts it back.
    lF.Tween->Keys[1].Pose.Scale = { 2.f, 2.f };
    lF.Tween->Play();
    lF.Run(1.f);
    CHECK(lF.Finished == 2);
    CHECK(lF.Scale() == doctest::Approx(2.f));
    lF.Tween->Stop();
    CHECK(lF.Scale() == doctest::Approx(1.f));

    // Stopped while playing: at rest, asleep, and no OnFinished.
    lF.Tween->Play();
    lF.Run(0.1f);
    lF.Tween->Stop();
    CHECK_FALSE(lF.Tween->IsPlaying());
    CHECK_FALSE(lF.Tween->IsAwake());
    CHECK(lF.Canvas.GetAwakeCount() == 0u);
    lF.Run(1.f);
    CHECK(lF.Finished == 2);
    CHECK(lF.Scale() == doctest::Approx(1.f));
}

TEST_CASE("UITweenModifier: a loop never finishes")
{
    TweenFixture lF;
    lF.Tween->Loop = EUILoop::PingPong;
    lF.Tween->Play();
    lF.Run(5.f);
    CHECK(lF.Tween->IsPlaying());
    CHECK(lF.Finished == 0);
    CHECK(lF.Canvas.GetAwakeCount() == 1u);
}

TEST_CASE("UITweenModifier: the same pose at the same moment at 60 and 240 Hz")
{
    TweenFixture l60;
    TweenFixture l240;
    l60.Tween->Keys[1].Ease  = EEase::BackOut;
    l240.Tween->Keys[1].Ease = EEase::BackOut;
    l60.Tween->Play();
    l240.Tween->Play();

    l60.Run(0.2f, 60.f);
    l240.Run(0.2f, 240.f);
    CHECK(l60.Scale() == doctest::Approx(l240.Scale()).epsilon(1e-4));
}

TEST_CASE("UITweenModifier: a Tween asset's keys win over its own; a Show tween plays when shown on a ticked canvas")
{
    TweenFixture lF;

    struct OneTween final : IUIAssetProvider
    {
        const Asset* Tween = nullptr;
        FontFaceView ResolveFace(const char*) override { return {}; }
        ITexture2D*  ResolveTexture(const char*) override { return nullptr; }
        const Asset* ResolveAsset(const char* InPath) override
        {
            return OpaaxString(InPath) == OpaaxString("UI/PopIn.opaaxasset") ? Tween : nullptr;
        }
    };

    TStrongObjectPtr<UITween> lAsset = NewObject<UITween>();
    lAsset->Keys = { Key(0.f, 3.f), Key(0.1f, 3.f) };
    OneTween lAssets;
    lAssets.Tween = lAsset.Get();
    lF.Canvas.Update(UIBuildContext{ &lAssets });

    lF.Tween->SetTween("UI/PopIn.opaaxasset");
    CHECK(lF.Tween->GetTween() == lAsset.Get());
    lF.Tween->Play();
    lF.Canvas.Update(UIBuildContext{ &lAssets });
    CHECK(lF.Image->GetDrawTransform().Decompose().Scale.x == doctest::Approx(3.f));

    // A Show tween plays when its widget joins a ticked canvas shown...
    UICanvas lGame{ 1080.f };
    auto lPanel = NewObject<UIPanel>();
    UITweenModifier& lShow = lPanel->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Show);
    lShow.Keys = Grow();
    lGame.Root().AddChild(Move(lPanel));
    CHECK(lShow.IsPlaying());

    // ...not on a canvas nobody ticks (an editor's document)...
    UICanvas lDocument{ 1080.f };
    lDocument.SetTicked(false);
    auto lOther = NewObject<UIPanel>();
    UITweenModifier& lStill = lOther->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Show);
    lDocument.Root().AddChild(Move(lOther));
    CHECK_FALSE(lStill.IsPlaying());

    // ...nor hidden; and a Manual one never by itself.
    auto lHidden = NewObject<UIPanel>();
    lHidden->bVisible = false;
    UITweenModifier& lHiddenTween = lHidden->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Show);
    lGame.Root().AddChild(Move(lHidden));
    CHECK_FALSE(lHiddenTween.IsPlaying());

    auto lManual = NewObject<UIPanel>();
    UITweenModifier& lManualTween = lManual->AddModifier<UITweenModifier>();
    lGame.Root().AddChild(Move(lManual));
    CHECK_FALSE(lManualTween.IsPlaying());
}

// =============================================================================
// Show and hide
// =============================================================================

namespace
{
    /** An image that fades out over 0.2 s when hidden (FadeOut, held at its end). */
    struct HideFixture
    {
        UICanvas         Canvas{ 1080.f };
        UIWidget*        Image = nullptr;
        UITweenModifier* Fade  = nullptr;

        HideFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Image = Canvas.Root().AddChild(NewObject<UIImage>());
            UIRect lRect;
            lRect.SizeDelta = { 100.f, 100.f };
            Image->SetRect(lRect);

            UITweenKey lFrom;
            UITweenKey lTo;
            lTo.Time         = 0.2f;
            lTo.Pose.Opacity = 0.f;
            Fade = &Image->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Hide);
            Fade->Keys = { lFrom, lTo };
            Canvas.Update();
        }

        void Run(const float InSeconds)
        {
            const Int32 lFrames = static_cast<Int32>(InSeconds * 60.f + 0.5f);
            for (Int32 lFrame = 0; lFrame < lFrames; ++lFrame)
            {
                Canvas.Tick(1.f / 60.f);
                Canvas.Update();
            }
        }
    };
}

TEST_CASE("UIWidget::Hide: without a hide tween, hidden at once; writing bVisible stays instant")
{
    TweenFixture lF;   // its tween plays on demand only
    lF.Image->Hide();
    CHECK_FALSE(lF.Image->bVisible);
    CHECK_FALSE(lF.Image->IsHiding());

    lF.Image->Show();
    CHECK(lF.Image->bVisible);

    HideFixture lH;
    lH.Image->bVisible = false;   // the instant way: no tween, no hiding
    CHECK_FALSE(lH.Fade->IsPlaying());
    CHECK_FALSE(lH.Image->IsHiding());
    lH.Image->bVisible = true;
}

TEST_CASE("UIWidget::Hide: with a hide tween, hidden when it ends, drawn but not hit meanwhile")
{
    HideFixture lH;
    CHECK(lH.Canvas.HitTest({ 0.f, 0.f }) == lH.Image);

    lH.Image->Hide();
    CHECK(lH.Image->IsHiding());
    CHECK(lH.Image->bVisible);
    CHECK(lH.Fade->IsPlaying());
    CHECK(lH.Canvas.HitTest({ 0.f, 0.f }) == nullptr);

    lH.Run(0.1f);
    CHECK(lH.Image->IsHiding());
    CHECK(lH.Image->GetDrawnOpacity() == doctest::Approx(0.5f).epsilon(1e-3));
    TDynArray<UIDrawItem> lItems;
    lH.Canvas.BuildDrawList(lItems);
    CHECK(lItems.size() == 1u);   // still drawn, fading

    lH.Run(0.2f);
    CHECK_FALSE(lH.Image->IsHiding());
    CHECK_FALSE(lH.Image->bVisible);
    lH.Canvas.BuildDrawList(lItems);
    CHECK(lItems.empty());

    // A second Hide does nothing; Show brings it back without the faded pose.
    lH.Image->Hide();
    CHECK_FALSE(lH.Image->IsHiding());
    lH.Image->Show();
    lH.Canvas.Update();
    CHECK(lH.Image->bVisible);
    CHECK(lH.Image->GetDrawnOpacity() == doctest::Approx(1.f));
    CHECK(lH.Canvas.HitTest({ 0.f, 0.f }) == lH.Image);
}

TEST_CASE("UIWidget::Show during a hide cancels it and drops its pose")
{
    HideFixture lH;
    lH.Image->Hide();
    lH.Run(0.1f);

    lH.Image->Show();
    CHECK_FALSE(lH.Image->IsHiding());
    CHECK(lH.Image->bVisible);
    CHECK_FALSE(lH.Fade->IsPlaying());
    lH.Canvas.Update();
    CHECK(lH.Image->GetDrawnOpacity() == doctest::Approx(1.f));

    // The cancelled hide never comes back to hide it.
    lH.Run(0.5f);
    CHECK(lH.Image->bVisible);
}

TEST_CASE("UIWidget::Show plays the Show tweens of the shown widgets under it; a parent's hide takes its children")
{
    TweenFixture lF;
    UIWidget* lChild = lF.Image->AddChild(NewObject<UIImage>());
    UITweenModifier& lChildShow = lChild->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Show);
    lChildShow.Keys = Grow();

    UIWidget* lHiddenChild = lF.Image->AddChild(NewObject<UIImage>());
    lHiddenChild->bVisible = false;
    UITweenModifier& lHiddenShow = lHiddenChild->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Show);
    lHiddenShow.Keys = Grow();

    // Joining the canvas played the shown one already: stop it, then Show the parent.
    lChildShow.Stop();
    lF.Image->Show();
    CHECK(lChildShow.IsPlaying());
    CHECK_FALSE(lHiddenShow.IsPlaying());

    // A child's hide tween does not play when its parent hides: it goes with the parent.
    UITweenModifier& lChildHide = lChild->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Hide);
    lChildHide.Keys = Grow();
    lF.Image->Hide();
    CHECK_FALSE(lF.Image->bVisible);
    CHECK_FALSE(lChildHide.IsPlaying());
    CHECK_FALSE(lChild->IsShownInTree());
}

TEST_CASE("UIWidget::Hide: a hide tween removed while it plays lets the hide end")
{
    HideFixture lH;
    lH.Image->Hide();
    REQUIRE(lH.Image->IsHiding());

    lH.Image->RemoveModifier(*lH.Fade);
    CHECK_FALSE(lH.Image->IsHiding());
    CHECK_FALSE(lH.Image->bVisible);
}

// =============================================================================
// Tweens on states
// =============================================================================
TEST_CASE("UITween: a repeating tween's cycle, and when the cycle under way ends")
{
    const TDynArray<UITweenKey> lKeys = { Key(0.f, 1.f), Key(0.5f, 1.5f), Key(1.f, 1.f) };
    CHECK(TweenCycleLength(lKeys, EUILoop::Loop) == doctest::Approx(1.f));
    CHECK(TweenCycleLength(lKeys, EUILoop::PingPong) == doctest::Approx(2.f));
    CHECK(TweenCycleLength(lKeys, EUILoop::Once) == doctest::Approx(0.f));
    CHECK(TweenCycleLength({}, EUILoop::Loop) == doctest::Approx(0.f));

    CHECK(CycleEndAfter(0.3f, 1.f) == doctest::Approx(1.f));
    CHECK(CycleEndAfter(1.f, 1.f) == doctest::Approx(1.f));    // at a cycle's end already
    CHECK(CycleEndAfter(1.2f, 1.f) == doctest::Approx(2.f));
    CHECK(CycleEndAfter(0.f, 1.f) == doctest::Approx(0.f));
    CHECK(CycleEndAfter(0.3f, 0.f) == doctest::Approx(0.3f));
}

namespace
{
    /** A pulse: rest, 1.5 at 0.5 s, rest at 1 s, looping. */
    TDynArray<UITweenKey> Pulse()
    {
        return { Key(0.f, 1.f), Key(0.5f, 1.5f), Key(1.f, 1.f) };
    }

    /** A 200x80 button at the origin with a looping pulse tween played on Hovered. */
    struct StateTweenFixture
    {
        UICanvas         Canvas{ 1080.f };
        UIButton*        Button = nullptr;
        UITweenModifier* Tween  = nullptr;

        StateTweenFixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            UIRect lRect;
            lRect.SizeDelta = { 200.f, 80.f };
            Button->SetRect(lRect);

            Tween = &Button->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Hovered);
            Tween->Keys = Pulse();
            Tween->Loop = EUILoop::Loop;
            Canvas.Update();
        }

        void Hover()   { Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 0.f, 0.f } }); }
        void Unhover() { Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 900.f, 500.f } }); }

        void Run(const float InSeconds)
        {
            const Int32 lFrames = static_cast<Int32>(InSeconds * 60.f + 0.5f);
            for (Int32 lFrame = 0; lFrame < lFrames; ++lFrame)
            {
                Canvas.Tick(1.f / 60.f);
                Canvas.Update();
            }
        }

        float Scale() const { return Button->GetDrawTransform().Decompose().Scale.x; }
    };
}

TEST_CASE("UITweenModifier: a state tween plays when its widget enters the state")
{
    StateTweenFixture lF;
    CHECK_FALSE(lF.Tween->IsPlaying());

    lF.Hover();
    CHECK(lF.Tween->IsPlaying());
    lF.Run(0.25f);
    CHECK(lF.Scale() == doctest::Approx(1.25f).epsilon(0.02));
}

TEST_CASE("UITweenModifier: a loop whose state ends finishes its cycle, then rests where it started")
{
    StateTweenFixture lF;
    lF.Hover();
    lF.Run(0.25f);
    lF.Unhover();
    CHECK(lF.Tween->IsPlaying());
    CHECK(lF.Tween->IsEnding());

    lF.Run(0.5f);                                   // 0.75 s: on its way back down, no jump
    CHECK(lF.Tween->IsPlaying());
    CHECK(lF.Scale() == doctest::Approx(1.25f).epsilon(0.02));

    lF.Run(0.3f);                                   // past 1 s: the cycle is done
    CHECK_FALSE(lF.Tween->IsPlaying());
    CHECK(lF.Scale() == doctest::Approx(1.f));
    CHECK_FALSE(lF.Tween->IsAwake());
}

TEST_CASE("UITweenModifier: entering its state again mid-cycle keeps it going, not from the start")
{
    StateTweenFixture lF;
    lF.Hover();
    lF.Run(0.25f);
    lF.Unhover();
    lF.Run(0.1f);
    const float lTime = lF.Tween->GetTime();

    lF.Hover();
    CHECK(lF.Tween->IsPlaying());
    CHECK_FALSE(lF.Tween->IsEnding());
    CHECK(lF.Tween->GetTime() == doctest::Approx(lTime));   // not reset

    lF.Run(1.f);                                    // well past where the old cycle would have ended
    CHECK(lF.Tween->IsPlaying());
}

TEST_CASE("UITweenModifier: a Once tween on Pressed plays to its end after the press ends")
{
    StateTweenFixture lF;
    UITweenModifier& lBounce = lF.Button->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Pressed);
    lBounce.Keys = { Key(0.f, 1.f), Key(0.2f, 0.9f), Key(0.4f, 1.f) };
    Int32 lFinished = 0;
    lBounce.OnFinished.Add([&lFinished]() { ++lFinished; });

    lF.Hover();
    lF.Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Down, { 0.f, 0.f }, EUIPointerButton::Primary });
    CHECK(lBounce.IsPlaying());
    lF.Run(0.1f);
    lF.Canvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Up, { 0.f, 0.f }, EUIPointerButton::Primary });
    CHECK(lBounce.IsPlaying());                     // released early: it goes on
    CHECK_FALSE(lBounce.IsEnding());

    lF.Run(0.5f);
    CHECK_FALSE(lBounce.IsPlaying());
    CHECK(lFinished == 1);
}

TEST_CASE("UITweenModifier: a state tween on a follower plays on its button's state")
{
    StateTweenFixture lF;
    UIWidget* lIcon = lF.Button->AddChild(NewObject<UIImage>());
    lIcon->bHitTestable = false;
    lIcon->SetStateSource(EUIStateSource::Button);
    UITweenModifier& lIconPulse = lIcon->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Hovered);
    lIconPulse.Keys = Pulse();
    lIconPulse.Loop = EUILoop::Loop;
    lF.Canvas.Update();

    lF.Hover();
    CHECK(lIconPulse.IsPlaying());
    lF.Unhover();
    CHECK(lIconPulse.IsEnding());
}

TEST_CASE("UITweenModifier: a widget already in its state when shown plays it (a button loaded disabled)")
{
    UICanvas lCanvas{ 1080.f };
    lCanvas.SetTargetSize(1920, 1080);

    auto lButton = NewObject<UIButton>();
    lButton->SetEnabled(false);
    UITweenModifier& lDim = lButton->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Disabled);
    lDim.Keys = Pulse();
    lDim.Loop = EUILoop::Loop;

    lCanvas.Root().AddChild(Move(lButton));   // joins a ticked canvas shown
    CHECK(lDim.IsPlaying());
}
