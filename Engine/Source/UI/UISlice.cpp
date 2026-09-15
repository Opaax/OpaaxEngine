#include "UI/UISlice.h"

#include <algorithm>

namespace Opaax
{
    namespace
    {
        /** Keep a pair of opposite edges inside InExtent, scaling both so their ratio survives. */
        void FitPair(float& InOutLow, float& InOutHigh, const float InExtent) noexcept
        {
            InOutLow  = std::max(0.f, InOutLow);
            InOutHigh = std::max(0.f, InOutHigh);

            const float lSum = InOutLow + InOutHigh;
            if (lSum > InExtent && lSum > 0.f)
            {
                const float lScale = InExtent / lSum;
                InOutLow  *= lScale;
                InOutHigh *= lScale;
            }
        }
    }

    void BuildSlicedQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels,
                          const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads)
    {
        const Vector2F lMin  = InRect.Min();
        const Vector2F lMax  = InRect.Max();
        const Vector2F lSize = InRect.Size();

        if (InTextureSize.x <= 0.f || InTextureSize.y <= 0.f)
        {
            UIQuad& lWhole = OutQuads.emplace_back();
            lWhole.Bounds  = InRect;
            return;
        }

        // The UVs keep the AUTHORED border (only fitted into the texture itself); the geometry is
        // fitted into the rect. That is the asymmetry 9-slice rests on — a squeezed widget
        // compresses its corner art rather than losing it.
        float lUvLeft = InBorderPixels.Left, lUvRight = InBorderPixels.Right;
        FitPair(lUvLeft, lUvRight, InTextureSize.x);

        float lUvBottom = InBorderPixels.Bottom, lUvTop = InBorderPixels.Top;
        FitPair(lUvBottom, lUvTop, InTextureSize.y);

        float lLeft = InBorderPixels.Left, lRight = InBorderPixels.Right;
        FitPair(lLeft, lRight, lSize.x);

        float lBottom = InBorderPixels.Bottom, lTop = InBorderPixels.Top;
        FitPair(lBottom, lTop, lSize.y);

        const float lXs[4] = { lMin.x, lMin.x + lLeft, lMax.x - lRight, lMax.x };
        const float lYs[4] = { lMin.y, lMin.y + lBottom, lMax.y - lTop, lMax.y };

        const float lUs[4] = { 0.f, lUvLeft / InTextureSize.x, 1.f - lUvRight / InTextureSize.x, 1.f };
        const float lVs[4] = { 0.f, lUvBottom / InTextureSize.y, 1.f - lUvTop / InTextureSize.y, 1.f };

        for (Int32 lRow = 0; lRow < 3; ++lRow)
        {
            if (lYs[lRow + 1] <= lYs[lRow])
            {
                continue;
            }

            for (Int32 lCol = 0; lCol < 3; ++lCol)
            {
                if (lXs[lCol + 1] <= lXs[lCol])
                {
                    continue;
                }

                UIQuad& lQuad = OutQuads.emplace_back();
                lQuad.Bounds  = Bounds2D::FromMinMax({ lXs[lCol], lYs[lRow] }, { lXs[lCol + 1], lYs[lRow + 1] });
                lQuad.UVMin   = { lUs[lCol], lVs[lRow] };
                lQuad.UVMax   = { lUs[lCol + 1], lVs[lRow + 1] };
            }
        }
    }

    void ClipQuadsTo(TDynArray<UIQuad>& InOutQuads, const Bounds2D& InClip)
    {
        const Vector2F lClipMin = InClip.Min();
        const Vector2F lClipMax = InClip.Max();

        Uint64 lKept = 0;

        for (UIQuad& lQuad : InOutQuads)
        {
            const Vector2F lQuadMin = lQuad.Bounds.Min();
            const Vector2F lQuadMax = lQuad.Bounds.Max();

            const Vector2F lNewMin{ std::max(lQuadMin.x, lClipMin.x), std::max(lQuadMin.y, lClipMin.y) };
            const Vector2F lNewMax{ std::min(lQuadMax.x, lClipMax.x), std::min(lQuadMax.y, lClipMax.y) };

            if (lNewMax.x <= lNewMin.x || lNewMax.y <= lNewMin.y)
            {
                continue;
            }

            const Vector2F lQuadSize = lQuadMax - lQuadMin;
            const Vector2F lUVSize   = lQuad.UVMax - lQuad.UVMin;

            if (lQuadSize.x > 0.f)
            {
                lQuad.UVMin.x = lQuad.UVMin.x + lUVSize.x * ((lNewMin.x - lQuadMin.x) / lQuadSize.x);
                lQuad.UVMax.x = lQuad.UVMin.x + lUVSize.x * ((lNewMax.x - lNewMin.x) / lQuadSize.x);
            }

            if (lQuadSize.y > 0.f)
            {
                lQuad.UVMin.y = lQuad.UVMin.y + lUVSize.y * ((lNewMin.y - lQuadMin.y) / lQuadSize.y);
                lQuad.UVMax.y = lQuad.UVMin.y + lUVSize.y * ((lNewMax.y - lNewMin.y) / lQuadSize.y);
            }

            lQuad.Bounds = Bounds2D::FromMinMax(lNewMin, lNewMax);

            // Compacted in place: a clip runs per rebuild, and the dropped quads are the minority.
            InOutQuads[lKept] = lQuad;
            ++lKept;
        }

        InOutQuads.resize(lKept);
    }

    void MapQuadUVsInto(TDynArray<UIQuad>& InOutQuads, const Vector2F& InUVMin, const Vector2F& InUVMax) noexcept
    {
        const Vector2F lSpan = InUVMax - InUVMin;

        for (UIQuad& lQuad : InOutQuads)
        {
            lQuad.UVMin = InUVMin + lQuad.UVMin * lSpan;
            lQuad.UVMax = InUVMin + lQuad.UVMax * lSpan;
        }
    }
}
