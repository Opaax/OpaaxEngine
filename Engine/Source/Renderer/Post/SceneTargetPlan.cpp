#include "Renderer/Post/SceneTargetPlan.h"

#include "Core/Maths/Maths.h"

namespace Opaax
{
    namespace
    {
        /** One side of the scene target: the view's side scaled, rounded, 1 pixel at least. */
        Uint32 ScaleSide(const Uint32 InSide, const float InScale)
        {
            return static_cast<Uint32>(Maths::Max(Maths::RoundToInt32(static_cast<float>(InSide) * InScale), 1));
        }
    }

    SceneTargetPlan PlanSceneTarget(const Uint32 InWidth, const Uint32 InHeight, const bool bInLit,
                                    const SceneTargetSettings& InSettings, const Uint32 InMaxSamples)
    {
        SceneTargetPlan lPlan;
        lPlan.bLit    = bInLit;
        lPlan.Samples = Maths::Clamp(InSettings.Samples, 1u, Maths::Max(InMaxSamples, 1u));

        // A scale that is not a number counts as 1.
        const float lScale = Maths::IsFinite(InSettings.RenderScale)
                           ? Maths::Clamp(InSettings.RenderScale, MIN_RENDER_SCALE, MAX_RENDER_SCALE)
                           : 1.f;

        lPlan.bFXAATarget  = InSettings.bFXAA;
        lPlan.bSceneTarget = bInLit || lPlan.Samples > 1 || lPlan.bFXAATarget || lScale != 1.f;
        if (!lPlan.bSceneTarget)
        {
            return lPlan;
        }

        lPlan.Width  = ScaleSide(InWidth, lScale);
        lPlan.Height = ScaleSide(InHeight, lScale);
        lPlan.Format = bInLit ? ETextureFormat::RGBA16F : ETextureFormat::RGBA8;
        return lPlan;
    }
}
