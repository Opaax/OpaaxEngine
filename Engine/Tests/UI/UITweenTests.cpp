// Suite: UI tweens (UI/Animation/UITween.h). Sampling is stateless: the pose of a moment, whatever the
// frames before it. Before the first key its pose; between two, eased by the second's curve; past the
// end held or at rest; Loop and PingPong wrap. And the asset: its file round trip, its keys kept sorted.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"
#include "UI/Animation/UITween.h"

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
