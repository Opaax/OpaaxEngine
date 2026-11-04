#include "Platform/Window/DisplayMode.h"

#include "Core/Algo/Algo.h"

namespace Opaax
{
    namespace
    {
        /** Smaller first: by area, then width, then refresh rate. */
        bool IsBefore(const DisplayMode& InA, const DisplayMode& InB)
        {
            const Uint64 lAreaA = static_cast<Uint64>(InA.Width) * InA.Height;
            const Uint64 lAreaB = static_cast<Uint64>(InB.Width) * InB.Height;
            if (lAreaA != lAreaB)         { return lAreaA < lAreaB; }
            if (InA.Width != InB.Width)   { return InA.Width < InB.Width; }
            return InA.RefreshRate < InB.RefreshRate;
        }

        /** How far InMode's size is from InWidth x InHeight, in pixels along both sides. */
        Uint64 SizeDistance(const DisplayMode& InMode, const Uint32 InWidth, const Uint32 InHeight)
        {
            const Uint64 lDx = InMode.Width > InWidth ? InMode.Width - InWidth : InWidth - InMode.Width;
            const Uint64 lDy = InMode.Height > InHeight ? InMode.Height - InHeight : InHeight - InMode.Height;
            return lDx + lDy;
        }
    }

    TDynArray<DisplayMode> SortDisplayModes(TDynArray<DisplayMode> InModes)
    {
        Algo::Sort(InModes, IsBefore);
        (void)Algo::Unique(InModes);
        return InModes;
    }

    DisplayMode ChooseDisplayMode(const TDynArray<DisplayMode>& InModes, const Uint32 InWidth, const Uint32 InHeight)
    {
        if (InModes.empty())
        {
            return DisplayMode{ InWidth, InHeight, 0 };
        }

        const DisplayMode* lBest = &InModes.front();
        for (const DisplayMode& lMode : InModes)
        {
            const Uint64 lDistance     = SizeDistance(lMode, InWidth, InHeight);
            const Uint64 lBestDistance = SizeDistance(*lBest, InWidth, InHeight);
            if (lDistance < lBestDistance || (lDistance == lBestDistance && lMode.RefreshRate > lBest->RefreshRate))
            {
                lBest = &lMode;
            }
        }
        return *lBest;
    }

    Int32 FindScreenAt(const TDynArray<ScreenArea>& InScreens, const Int32 InX, const Int32 InY)
    {
        for (Uint64 lIndex = 0; lIndex < InScreens.size(); ++lIndex)
        {
            const ScreenArea& lScreen = InScreens[lIndex];
            const Int64       lRight  = static_cast<Int64>(lScreen.X) + lScreen.Width;
            const Int64       lBottom = static_cast<Int64>(lScreen.Y) + lScreen.Height;

            if (InX >= lScreen.X && InX < lRight && InY >= lScreen.Y && InY < lBottom)
            {
                return static_cast<Int32>(lIndex);
            }
        }
        return -1;
    }
}
