#include "UI/UISlice.h"

#include "Core/Maths/Maths.h"

namespace Opaax
{
    namespace
    {
        /** Keeps a pair of opposite edges inside InExtent, scaled to keep their ratio. */
        void FitPair(float& InOutLow, float& InOutHigh, const float InExtent) noexcept
        {
            InOutLow  = Maths::Max(0.f, InOutLow);
            InOutHigh = Maths::Max(0.f, InOutHigh);

            const float lSum = InOutLow + InOutHigh;
            if (lSum > InExtent && lSum > 0.f)
            {
                const float lScale = InExtent / lSum;
                InOutLow  *= lScale;
                InOutHigh *= lScale;
            }
        }

        /** One column or row of the grid: where it is on screen and which part of the image it shows. */
        struct SliceSpan
        {
            float From   = 0.f;
            float To     = 0.f;
            float UVFrom = 0.f;
            float UVTo   = 1.f;
        };

        /** One axis of a slice: the borders' ends on screen, the middle's UVs and its tile length. */
        struct SliceAxis
        {
            float Min = 0.f, Low = 0.f, High = 0.f, Max = 0.f;
            float UVLow = 0.f, UVHigh = 1.f;
            float Tile = 0.f;   // a middle tile on screen; 0: stretched
        };

        /** One axis: the UVs keep the authored border, the geometry is fitted into the rect. */
        SliceAxis MakeAxis(const float InMin, const float InMax, const float InLowPx, const float InHighPx,
                           const float InScale, const float InTexturePx) noexcept
        {
            float lUvLow = InLowPx, lUvHigh = InHighPx;
            FitPair(lUvLow, lUvHigh, InTexturePx);

            // On screen a border pixel takes InScale canvas units.
            float lLow = InLowPx * InScale, lHigh = InHighPx * InScale;
            FitPair(lLow, lHigh, InMax - InMin);

            SliceAxis lAxis;
            lAxis.Min    = InMin;
            lAxis.Low    = InMin + lLow;
            lAxis.High   = InMax - lHigh;
            lAxis.Max    = InMax;
            lAxis.UVLow  = lUvLow / InTexturePx;
            lAxis.UVHigh = 1.f - lUvHigh / InTexturePx;
            return lAxis;
        }

        /** How many spans the middle takes: 1 stretched, else as many tiles as cover it (capped). */
        Uint64 MiddleCount(const SliceAxis& InAxis) noexcept
        {
            const float lLength = InAxis.High - InAxis.Low;
            if (InAxis.Tile <= 0.f || lLength <= 0.f)
            {
                return 1;
            }

            // A last tile thinner than a thousandth of one is rounding: the one before it is stretched by that much.
            const float lCount = Maths::Ceil(lLength / InAxis.Tile - 0.001f);
            if (lCount > static_cast<float>(UI_MAX_TILED_QUADS))
            {
                return UI_MAX_TILED_QUADS + 1;
            }

            return lCount < 1.f ? 1 : static_cast<Uint64>(lCount);
        }

        /** The axis's spans, low to high; empty ones are left out. */
        void MakeSpans(const SliceAxis& InAxis, TDynArray<SliceSpan>& OutSpans)
        {
            auto lAdd = [&OutSpans](const float InFrom, const float InTo, const float InUVFrom, const float InUVTo)
            {
                if (InTo > InFrom)
                {
                    OutSpans.push_back({ InFrom, InTo, InUVFrom, InUVTo });
                }
            };

            lAdd(InAxis.Min, InAxis.Low, 0.f, InAxis.UVLow);

            if (InAxis.Tile <= 0.f)
            {
                lAdd(InAxis.Low, InAxis.High, InAxis.UVLow, InAxis.UVHigh);
            }
            else
            {
                const Uint64 lCount = MiddleCount(InAxis);
                for (Uint64 lIndex = 0; lIndex < lCount; ++lIndex)
                {
                    const float lFrom = InAxis.Low + InAxis.Tile * static_cast<float>(lIndex);
                    const float lTo   = lIndex + 1 == lCount ? InAxis.High : lFrom + InAxis.Tile;
                    const float lPart = Maths::Min(1.f, (lTo - lFrom) / InAxis.Tile);   // below 1 for a cut tile
                    lAdd(lFrom, lTo, InAxis.UVLow, InAxis.UVLow + (InAxis.UVHigh - InAxis.UVLow) * lPart);
                }
            }

            lAdd(InAxis.High, InAxis.Max, InAxis.UVHigh, 1.f);
        }

        /** The slice as a grid of columns and rows; false, with nothing appended, past the tile cap. */
        bool BuildGrid(const Bounds2D& InRect, const UIMargin& InBorderPixels, const float InBorderScale,
                       const Vector2F& InTextureSize, const bool bInTile, TDynArray<UIQuad>& OutQuads)
        {
            if (InTextureSize.x <= 0.f || InTextureSize.y <= 0.f)
            {
                OutQuads.emplace_back().Bounds = InRect;
                return true;
            }

            const float    lScale = SanitizeBorderScale(InBorderScale);
            const Vector2F lMin   = InRect.Min();
            const Vector2F lMax   = InRect.Max();

            SliceAxis lX = MakeAxis(lMin.x, lMax.x, InBorderPixels.Left, InBorderPixels.Right, lScale, InTextureSize.x);
            SliceAxis lY = MakeAxis(lMin.y, lMax.y, InBorderPixels.Bottom, InBorderPixels.Top, lScale, InTextureSize.y);

            if (bInTile)
            {
                lX.Tile = (lX.UVHigh - lX.UVLow) * InTextureSize.x * lScale;
                lY.Tile = (lY.UVHigh - lY.UVLow) * InTextureSize.y * lScale;

                // Counted before anything is made: a tiny scale would ask for millions.
                if (MiddleCount(lX) > UI_MAX_TILED_QUADS || MiddleCount(lY) > UI_MAX_TILED_QUADS)
                {
                    return false;
                }
            }

            TDynArray<SliceSpan> lColumns, lRows;
            MakeSpans(lX, lColumns);
            MakeSpans(lY, lRows);

            if (bInTile && lColumns.size() * lRows.size() > UI_MAX_TILED_QUADS)
            {
                return false;
            }

            for (const SliceSpan& lRow : lRows)
            {
                for (const SliceSpan& lColumn : lColumns)
                {
                    UIQuad& lQuad = OutQuads.emplace_back();
                    lQuad.Bounds  = Bounds2D::FromMinMax({ lColumn.From, lRow.From }, { lColumn.To, lRow.To });
                    lQuad.UVMin   = { lColumn.UVFrom, lRow.UVFrom };
                    lQuad.UVMax   = { lColumn.UVTo, lRow.UVTo };
                }
            }

            return true;
        }
    }

    float SanitizeBorderScale(const float InBorderScale) noexcept
    {
        return InBorderScale > 0.f ? InBorderScale : 1.f;   // a NaN fails the test too
    }

    void BuildSlicedQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels,
                          const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads)
    {
        BuildSlicedQuads(InRect, InBorderPixels, 1.f, InTextureSize, OutQuads);
    }

    void BuildSlicedQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels, const float InBorderScale,
                          const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads)
    {
        BuildGrid(InRect, InBorderPixels, InBorderScale, InTextureSize, false, OutQuads);
    }

    bool BuildTiledQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels, const float InBorderScale,
                         const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads)
    {
        return BuildGrid(InRect, InBorderPixels, InBorderScale, InTextureSize, true, OutQuads);
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

            const Vector2F lNewMin{ Maths::Max(lQuadMin.x, lClipMin.x), Maths::Max(lQuadMin.y, lClipMin.y) };
            const Vector2F lNewMax{ Maths::Min(lQuadMax.x, lClipMax.x), Maths::Min(lQuadMax.y, lClipMax.y) };

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

            // Compacted in place.
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
            lQuad.UVFrame = MakeUVBounds(InUVMin, InUVMax);
        }
    }
}
