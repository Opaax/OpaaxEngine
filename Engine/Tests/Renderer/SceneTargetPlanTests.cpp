// Suite: PlanSceneTarget — whether a world view goes through a scene target, and its size,
// format and samples. Pure: no device.
#include <doctest.h>

#include "Core/Templates/TypeTraits.h"
#include "Renderer/Post/SceneTargetPlan.h"

using namespace Opaax;

TEST_CASE("SceneTargetPlan: everything off and unlit, the world is drawn straight into its target")
{
    const SceneTargetPlan lPlan = PlanSceneTarget(1280, 720, false, SceneTargetSettings{}, 8);

    CHECK_FALSE(lPlan.bSceneTarget);
    CHECK_FALSE(lPlan.bFXAATarget);
    CHECK(lPlan.Samples == 1);
}

TEST_CASE("SceneTargetPlan: a lit world gets an RGBA16F target of the view's size")
{
    const SceneTargetPlan lPlan = PlanSceneTarget(1280, 720, true, SceneTargetSettings{}, 8);

    CHECK(lPlan.bSceneTarget);
    CHECK(lPlan.bLit);
    CHECK(lPlan.Format == ETextureFormat::RGBA16F);
    CHECK(lPlan.Width == 1280);
    CHECK(lPlan.Height == 720);
    CHECK(lPlan.Samples == 1);
}

TEST_CASE("SceneTargetPlan: each setting alone sends an unlit world through an RGBA8 target")
{
    SceneTargetSettings lSamples;
    lSamples.Samples = 4;

    SceneTargetSettings lFXAA;
    lFXAA.bFXAA = true;

    SceneTargetSettings lScale;
    lScale.RenderScale = 0.5f;

    for (const SceneTargetSettings& lSettings : { lSamples, lFXAA, lScale })
    {
        const SceneTargetPlan lPlan = PlanSceneTarget(1280, 720, false, lSettings, 8);
        CHECK(lPlan.bSceneTarget);
        CHECK_FALSE(lPlan.bLit);
        CHECK(lPlan.Format == ETextureFormat::RGBA8);
    }

    CHECK(PlanSceneTarget(1280, 720, false, lFXAA, 8).bFXAATarget);
    CHECK_FALSE(PlanSceneTarget(1280, 720, false, lSamples, 8).bFXAATarget);
}

TEST_CASE("SceneTargetPlan: samples are capped by the device's most")
{
    SceneTargetSettings lSettings;
    lSettings.Samples = 8;

    CHECK(PlanSceneTarget(100, 100, false, lSettings, 4).Samples == 4);
    CHECK(PlanSceneTarget(100, 100, false, lSettings, 16).Samples == 8);

    // A device without multisampling: one sample, so no scene target for that alone.
    const SceneTargetPlan lNone = PlanSceneTarget(100, 100, false, lSettings, 1);
    CHECK(lNone.Samples == 1);
    CHECK_FALSE(lNone.bSceneTarget);

    // A device that answers 0 counts as 1; 0 asked counts as 1.
    CHECK(PlanSceneTarget(100, 100, false, lSettings, 0).Samples == 1);
    lSettings.Samples = 0;
    CHECK(PlanSceneTarget(100, 100, true, lSettings, 8).Samples == 1);
}

TEST_CASE("SceneTargetPlan: the scale rounds, is clamped and keeps 1 pixel at least")
{
    SceneTargetSettings lSettings;

    lSettings.RenderScale = 0.75f;
    SceneTargetPlan lPlan = PlanSceneTarget(101, 3, false, lSettings, 1);
    CHECK(lPlan.Width == 76);    // 75.75
    CHECK(lPlan.Height == 2);    // 2.25

    lSettings.RenderScale = 10.f;
    lPlan = PlanSceneTarget(100, 50, false, lSettings, 1);
    CHECK(lPlan.Width == 200);
    CHECK(lPlan.Height == 100);

    lSettings.RenderScale = 0.01f;
    lPlan = PlanSceneTarget(100, 1, false, lSettings, 1);
    CHECK(lPlan.Width == 50);
    CHECK(lPlan.Height == 1);

    // A view of no size still gets a target of 1 pixel.
    lSettings.RenderScale = 1.f;
    lPlan = PlanSceneTarget(0, 0, true, lSettings, 1);
    CHECK(lPlan.Width == 1);
    CHECK(lPlan.Height == 1);
}

TEST_CASE("SceneTargetPlan: a scale that is not a number counts as 1")
{
    SceneTargetSettings lSettings;
    lSettings.RenderScale = TNumericLimits<float>::quiet_NaN();

    CHECK_FALSE(PlanSceneTarget(100, 100, false, lSettings, 1).bSceneTarget);

    const SceneTargetPlan lLit = PlanSceneTarget(100, 100, true, lSettings, 1);
    CHECK(lLit.Width == 100);
    CHECK(lLit.Height == 100);
}
