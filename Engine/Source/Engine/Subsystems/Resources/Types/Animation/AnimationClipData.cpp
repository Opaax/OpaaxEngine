#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipData.h"

namespace Opaax
{
    AnimationSample SampleClip(const AnimationClipData& InClip, const float InTimeSeconds)
    {
        const Uint32 lTotal = InClip.TotalTicks();

        // Nothing to show, or nothing that can ever advance. Finished either way: the honest
        // answer to "will this show anything else" is no, and it is what stops a caller
        // accumulating time forever.
        if (InClip.StepCount() == 0u || lTotal == 0u || InClip.Fps <= 0.f)
        {
            return AnimationSample{ 0u, true };
        }

        // Negative time cannot happen from a forward-running clock, but the function is total.
        const float lSeconds = (InTimeSeconds > 0.f) ? InTimeSeconds : 0.f;
        const Int64 lRawTick = static_cast<Int64>(lSeconds * InClip.Fps);   // floor, lSeconds >= 0

        Uint32 lTick      = 0u;
        bool   lbFinished = false;

        switch (InClip.PlayMode)
        {
        case EAnimPlayMode::Once:
            if (lRawTick >= static_cast<Int64>(lTotal))
            {
                lTick      = lTotal - 1u;
                lbFinished = true;
            }
            else
            {
                lTick = static_cast<Uint32>(lRawTick);
            }
            break;

        case EAnimPlayMode::Loop:
            lTick = static_cast<Uint32>(lRawTick % static_cast<Int64>(lTotal));
            break;

        case EAnimPlayMode::PingPong:
        {
            // A 4-tick clip walks 0 1 2 3 2 1 and repeats — a period of 2*4-2, not 2*4, because
            // the two ends are not held twice.
            const Uint32 lPeriod   = (lTotal > 1u) ? (2u * lTotal - 2u) : 1u;
            const Uint32 lPosition = static_cast<Uint32>(lRawTick % static_cast<Int64>(lPeriod));

            lTick = (lPosition < lTotal) ? lPosition : (lPeriod - lPosition);
            break;
        }
        }

        // Prefix-scan the holds. Steps are a handful, so this beats caching a table that would
        // have to be invalidated every time the clip is edited.
        Uint32 lAccumulated = 0u;

        for (Uint32 lIndex = 0u; lIndex < InClip.StepCount(); ++lIndex)
        {
            lAccumulated += InClip.Steps[lIndex].EffectiveHold();

            if (lTick < lAccumulated)
            {
                return AnimationSample{ lIndex, lbFinished };
            }
        }

        return AnimationSample{ InClip.StepCount() - 1u, lbFinished };
    }
}
