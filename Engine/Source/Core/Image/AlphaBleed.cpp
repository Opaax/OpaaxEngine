#include "Core/Image/AlphaBleed.h"

namespace Opaax
{
    namespace
    {
        // Distances in thirds of a texel: 3 to a side neighbour, 4 to a corner one.
        constexpr Uint16 FAR_AWAY    = 0xFFFF;
        constexpr Uint32 SIDE_COST   = 3;
        constexpr Uint32 CORNER_COST = 4;

        /** An image being swept: each texel takes the colour of a neighbour that is nearer to a visible texel. */
        struct BleedSweep
        {
            Uint8*            Pixels;
            TDynArray<Uint16> Distance;
            Int64             Width;
            Int64             Height;

            void TakeFrom(const Int64 InX, const Int64 InY, const Int64 InFromX, const Int64 InFromY, const Uint32 InCost)
            {
                if (InFromX < 0 || InFromX >= Width || InFromY < 0 || InFromY >= Height)
                {
                    return;
                }

                const size_t lTo    = static_cast<size_t>(InY * Width + InX);
                const size_t lFrom  = static_cast<size_t>(InFromY * Width + InFromX);
                const Uint32 lReach = static_cast<Uint32>(Distance[lFrom]) + InCost;

                if (lReach < Distance[lTo])
                {
                    Distance[lTo]       = static_cast<Uint16>(lReach);
                    Pixels[lTo * 4 + 0] = Pixels[lFrom * 4 + 0];
                    Pixels[lTo * 4 + 1] = Pixels[lFrom * 4 + 1];
                    Pixels[lTo * 4 + 2] = Pixels[lFrom * 4 + 2];
                }
            }
        };
    }

    Uint32 BleedAlphaEdges(Uint8* InOutRGBA, const Uint32 InWidth, const Uint32 InHeight)
    {
        if (InOutRGBA == nullptr || InWidth == 0 || InHeight == 0)
        {
            return 0;
        }

        const size_t lCount = static_cast<size_t>(InWidth) * InHeight;

        BleedSweep lSweep{ InOutRGBA, TDynArray<Uint16>(lCount, FAR_AWAY), InWidth, InHeight };

        size_t lVisible = 0;
        for (size_t i = 0; i < lCount; ++i)
        {
            if (InOutRGBA[i * 4 + 3] != 0)
            {
                lSweep.Distance[i] = 0;
                ++lVisible;
            }
        }

        if (lVisible == 0 || lVisible == lCount)
        {
            return 0;
        }

        // Two sweeps reach every texel: down and right, taking from the texels already passed,
        // then back up and left.
        for (Int64 lY = 0; lY < lSweep.Height; ++lY)
        {
            for (Int64 lX = 0; lX < lSweep.Width; ++lX)
            {
                lSweep.TakeFrom(lX, lY, lX - 1, lY,     SIDE_COST);
                lSweep.TakeFrom(lX, lY, lX,     lY - 1, SIDE_COST);
                lSweep.TakeFrom(lX, lY, lX - 1, lY - 1, CORNER_COST);
                lSweep.TakeFrom(lX, lY, lX + 1, lY - 1, CORNER_COST);
            }
        }

        for (Int64 lY = lSweep.Height - 1; lY >= 0; --lY)
        {
            for (Int64 lX = lSweep.Width - 1; lX >= 0; --lX)
            {
                lSweep.TakeFrom(lX, lY, lX + 1, lY,     SIDE_COST);
                lSweep.TakeFrom(lX, lY, lX,     lY + 1, SIDE_COST);
                lSweep.TakeFrom(lX, lY, lX + 1, lY + 1, CORNER_COST);
                lSweep.TakeFrom(lX, lY, lX - 1, lY + 1, CORNER_COST);
            }
        }

        Uint32 lBled = 0;
        for (size_t i = 0; i < lCount; ++i)
        {
            if (lSweep.Distance[i] != 0 && lSweep.Distance[i] != FAR_AWAY)
            {
                ++lBled;
            }
        }
        return lBled;
    }
}
