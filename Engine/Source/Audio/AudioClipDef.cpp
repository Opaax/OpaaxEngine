#include "Audio/AudioClipDef.h"

#include <algorithm>

#include "Audio/AudioRandom.h"
#include "Core/Maths/Maths.h"

namespace Opaax
{
    namespace
    {
        /**
         * A clip by weight, InExcluded left out (-1: none). Weights at or below 0 count as 0; when
         * every candidate weighs 0, they are equally likely.
         */
        Int32 PickWeighted(const TDynArray<AudioClipEntry>& InClips, const Int32 InExcluded, AudioRandom& InRandom)
        {
            const Int32 lCount = static_cast<Int32>(InClips.size());

            float  lTotal      = 0.f;
            Uint32 lCandidates = 0;
            for (Int32 lIndex = 0; lIndex < lCount; ++lIndex)
            {
                if (lIndex == InExcluded) { continue; }
                lTotal += Maths::Max(InClips[lIndex].Weight, 0.f);
                ++lCandidates;
            }

            if (lCandidates == 0)
            {
                return -1;
            }

            if (lTotal <= 0.f)
            {
                Uint32 lPick = InRandom.Below(lCandidates);
                for (Int32 lIndex = 0; lIndex < lCount; ++lIndex)
                {
                    if (lIndex == InExcluded) { continue; }
                    if (lPick-- == 0) { return lIndex; }
                }
            }

            float lPick = InRandom.Range(0.f, lTotal);
            Int32 lLast = -1;
            for (Int32 lIndex = 0; lIndex < lCount; ++lIndex)
            {
                const float lWeight = Maths::Max(InClips[lIndex].Weight, 0.f);
                if (lIndex == InExcluded || lWeight <= 0.f) { continue; }

                lLast = lIndex;
                if (lPick < lWeight) { return lIndex; }
                lPick -= lWeight;
            }

            // Rounding left the pick past the end: the last one with a weight.
            return lLast;
        }

        /** A new shuffled order of every clip; it does not start with InAvoid when it can. */
        void Refill(TDynArray<Uint32>& OutBag, const Uint32 InCount, const Int32 InAvoid, AudioRandom& InRandom)
        {
            OutBag.resize(InCount);
            for (Uint32 lIndex = 0; lIndex < InCount; ++lIndex)
            {
                OutBag[lIndex] = lIndex;
            }

            // Fisher-Yates; the bag is drawn from its back.
            for (Uint32 lIndex = InCount; lIndex > 1; --lIndex)
            {
                Swap(OutBag[lIndex - 1], OutBag[InRandom.Below(lIndex)]);
            }

            if (InCount > 1 && static_cast<Int32>(OutBag.back()) == InAvoid)
            {
                Swap(OutBag.back(), OutBag.front());
            }
        }
    }

    Int32 SelectAudioClip(const AudioClipDef& InDef, AudioSelectionState& InOutState, AudioRandom& InRandom)
    {
        const Uint32 lCount = static_cast<Uint32>(InDef.Clips.size());
        if (lCount == 0)
        {
            return -1;
        }

        Int32 lChosen = 0;
        switch (InDef.Selection)
        {
        case EAudioSelection::Single:
            lChosen = 0;
            break;

        case EAudioSelection::Random:
            lChosen = PickWeighted(InDef.Clips, -1, InRandom);
            break;

        case EAudioSelection::RandomNoRepeat:
            lChosen = PickWeighted(InDef.Clips, lCount > 1 ? InOutState.Last : -1, InRandom);
            if (lChosen < 0)
            {
                // Only the last one has a weight: it plays again rather than nothing.
                lChosen = PickWeighted(InDef.Clips, -1, InRandom);
            }
            break;

        case EAudioSelection::Sequential:
            lChosen = static_cast<Int32>(InOutState.Next % lCount);
            InOutState.Next = (InOutState.Next + 1) % lCount;
            break;

        case EAudioSelection::Shuffle:
            // Clips added or removed since the last draw: a new order.
            if (InOutState.Bag.empty() || std::any_of(InOutState.Bag.begin(), InOutState.Bag.end(),
                                                      [lCount](const Uint32 InIndex) { return InIndex >= lCount; }))
            {
                Refill(InOutState.Bag, lCount, InOutState.Last, InRandom);
            }
            lChosen = static_cast<Int32>(InOutState.Bag.back());
            InOutState.Bag.pop_back();
            break;
        }

        InOutState.Last = lChosen;
        return lChosen;
    }

    PlaySoundParams MakePlaySoundParams(const AudioClipDef& InDef, const Int32 InIndex, AudioRandom& InRandom)
    {
        const AudioClipEntry* lEntry = (InIndex >= 0 && InIndex < static_cast<Int32>(InDef.Clips.size()))
                                     ? &InDef.Clips[InIndex] : nullptr;

        const float lVolumeSpread = Maths::Clamp(InDef.VolumeSpread, 0.f, 1.f);
        const float lPitchSpread  = Maths::Clamp(InDef.PitchSpread, 0.f, 1.f);

        PlaySoundParams lParams;
        lParams.Bus         = InDef.Bus;
        lParams.Volume      = InDef.Volume * (lEntry != nullptr ? lEntry->VolumeScale : 1.f)
                            * (lVolumeSpread > 0.f ? 1.f + InRandom.Range(-lVolumeSpread, lVolumeSpread) : 1.f);
        lParams.Pitch       = InDef.Pitch * (lEntry != nullptr ? lEntry->PitchScale : 1.f)
                            * (lPitchSpread > 0.f ? 1.f + InRandom.Range(-lPitchSpread, lPitchSpread) : 1.f);
        lParams.bLoop       = InDef.bLoop;
        lParams.Priority    = static_cast<Uint8>(Maths::Min<Uint32>(InDef.Priority, 255u));
        lParams.bSpatial    = InDef.bSpatial;
        lParams.MinDistance = InDef.MinDistance;
        lParams.MaxDistance = InDef.MaxDistance;
        lParams.Attenuation = InDef.Attenuation;
        return lParams;
    }
}
