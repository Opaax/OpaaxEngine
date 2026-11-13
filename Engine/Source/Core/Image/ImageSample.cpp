#include "Core/Image/ImageSample.h"

namespace Opaax::ImageSample
{
    bool Measure(const Uint8* InRGBA, const Uint32 InWidth, const Uint32 InHeight, const Rect& InArea,
                 Result& OutResult, OpaaxString& OutError)
    {
        if (InRGBA == nullptr || InWidth == 0 || InHeight == 0)
        {
            OutError = "there is no image to read";
            return false;
        }

        if (InArea.Width <= 0 || InArea.Height <= 0)
        {
            OutError = "\"width\" and \"height\" are the size of the area in pixels: give each 1 or more";
            return false;
        }

        // 64-bit: a far corner plus a size must not wrap.
        const Int64 lLeft   = InArea.X > 0 ? InArea.X : 0;
        const Int64 lTop    = InArea.Y > 0 ? InArea.Y : 0;
        Int64       lRight  = static_cast<Int64>(InArea.X) + InArea.Width;
        Int64       lBottom = static_cast<Int64>(InArea.Y) + InArea.Height;
        if (lRight  > InWidth)  { lRight  = InWidth; }
        if (lBottom > InHeight) { lBottom = InHeight; }

        if (lLeft >= lRight || lTop >= lBottom)
        {
            OutError = OpaaxString::Format("the area is outside the image, which is {} by {} pixels from (0, 0) at its "
                                           "top-left", InWidth, InHeight);
            return false;
        }

        OutResult      = Result{};
        OutResult.Area = Rect{ static_cast<Int32>(lLeft), static_cast<Int32>(lTop),
                               static_cast<Int32>(lRight - lLeft), static_cast<Int32>(lBottom - lTop) };

        for (Uint32 lChannel = 0; lChannel < 4; ++lChannel)
        {
            OutResult.Min[lChannel] = 255;
        }

        TUnorderedSet<Uint32> lColors;
        Uint64                lSum[4] = {};

        for (Int64 lY = lTop; lY < lBottom; ++lY)
        {
            const Uint8* lPixel = InRGBA + (static_cast<size_t>(lY) * InWidth + static_cast<size_t>(lLeft)) * 4;

            for (Int64 lX = lLeft; lX < lRight; ++lX, lPixel += 4)
            {
                for (Uint32 lChannel = 0; lChannel < 4; ++lChannel)
                {
                    const Uint8 lValue = lPixel[lChannel];
                    lSum[lChannel] += lValue;
                    if (lValue < OutResult.Min[lChannel]) { OutResult.Min[lChannel] = lValue; }
                    if (lValue > OutResult.Max[lChannel]) { OutResult.Max[lChannel] = lValue; }
                }

                lColors.insert((static_cast<Uint32>(lPixel[0]) << 24) | (static_cast<Uint32>(lPixel[1]) << 16) |
                               (static_cast<Uint32>(lPixel[2]) << 8)  |  static_cast<Uint32>(lPixel[3]));
            }
        }

        const double lCount = static_cast<double>((lRight - lLeft) * (lBottom - lTop));
        for (Uint32 lChannel = 0; lChannel < 4; ++lChannel)
        {
            OutResult.Mean[lChannel] = static_cast<double>(lSum[lChannel]) / lCount;
        }

        OutResult.Colors = static_cast<Uint32>(lColors.size());
        return true;
    }
}
