#include "UI/Animation/UITween.h"

#include "Core/Algo/Algo.h"
#include "Core/Maths/Maths.h"

namespace Opaax
{
    UIPose SampleTweenKeys(const TDynArray<UITweenKey>& InKeys, const EUILoop InLoop, const bool bInHoldEnd,
                           const float InTime, bool& OutFinished)
    {
        OutFinished = false;

        if (InKeys.empty())
        {
            OutFinished = InLoop == EUILoop::Once;
            return UIPose{};
        }

        const float lLength = InKeys.back().Time;
        float       lTime   = Maths::Max(InTime, 0.f);

        // Where in one pass of the keys InTime falls.
        if (lLength > 0.f)
        {
            switch (InLoop)
            {
            case EUILoop::Once:
                if (lTime >= lLength)
                {
                    OutFinished = true;
                    return bInHoldEnd ? InKeys.back().Pose : UIPose{};
                }
                break;

            case EUILoop::Loop:
                lTime = Maths::FMod(lTime, lLength);
                break;

            case EUILoop::PingPong:
                lTime = Maths::FMod(lTime, lLength * 2.f);
                if (lTime > lLength)
                {
                    lTime = lLength * 2.f - lTime;   // on the way back
                }
                break;
            }
        }
        else if (InLoop == EUILoop::Once)
        {
            // Every key at 0: the tween is that pose, and done at once.
            OutFinished = true;
            return bInHoldEnd ? InKeys.back().Pose : UIPose{};
        }

        if (lTime <= InKeys.front().Time)
        {
            return InKeys.front().Pose;
        }

        // The first key past lTime: the motion runs from the one before it.
        for (Uint64 lIndex = 1; lIndex < InKeys.size(); ++lIndex)
        {
            const UITweenKey& lTo = InKeys[lIndex];
            if (lTime > lTo.Time)
            {
                continue;
            }

            const UITweenKey& lFrom = InKeys[lIndex - 1];
            const float       lSpan = lTo.Time - lFrom.Time;
            if (lSpan <= 0.f)
            {
                return lTo.Pose;
            }
            return UIPose::Lerp(lFrom.Pose, lTo.Pose, Easing::Evaluate(lTo.Ease, (lTime - lFrom.Time) / lSpan));
        }

        return InKeys.back().Pose;
    }

    void SortTweenKeys(TDynArray<UITweenKey>& InOutKeys)
    {
        Algo::StableSort(InOutKeys, [](const UITweenKey& InA, const UITweenKey& InB) { return InA.Time < InB.Time; });
    }

    void UITween::PostEditChange()
    {
        SortTweenKeys(Keys);
    }

    UIPose SampleTween(const UITween& InTween, const float InTime, bool& OutFinished)
    {
        return SampleTweenKeys(InTween.Keys, InTween.Loop, InTween.bHoldEnd, InTime, OutFinished);
    }

    float TweenCycleLength(const TDynArray<UITweenKey>& InKeys, const EUILoop InLoop) noexcept
    {
        const float lLength = InKeys.empty() ? 0.f : InKeys.back().Time;
        if (lLength <= 0.f)
        {
            return 0.f;
        }

        switch (InLoop)
        {
        case EUILoop::Loop:     return lLength;
        case EUILoop::PingPong: return lLength * 2.f;
        case EUILoop::Once:     break;
        }
        return 0.f;
    }

    float CycleEndAfter(const float InTime, const float InCycle) noexcept
    {
        if (InCycle <= 0.f || InTime <= 0.f)
        {
            return Maths::Max(InTime, 0.f);
        }
        return Maths::CeilToFloat(InTime / InCycle) * InCycle;
    }
}
