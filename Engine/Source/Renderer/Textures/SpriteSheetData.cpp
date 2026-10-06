#include "Renderer/Textures/SpriteSheetData.h"

namespace Opaax
{
    namespace
    {
        /** How many cells (with spacing) fit in InAvailable pixels. */
        Uint32 CellsThatFit(const float InAvailable, const float InCell, const float InSpacing)
        {
            if (InCell <= 0.f || InAvailable < InCell) { return 0u; }

            const float lPitch = InCell + InSpacing;
            if (lPitch <= 0.f) { return 0u; }

            return static_cast<Uint32>((InAvailable + InSpacing) / lPitch);
        }
    }

    SpriteUVRect MakeFrameUV(const SpriteFrame& InFrame, const Uint32 InTexWidth, const Uint32 InTexHeight)
    {
        const float lWidth  = static_cast<float>(InTexWidth);
        const float lHeight = static_cast<float>(InTexHeight);

        if (lWidth <= 0.f || lHeight <= 0.f || InFrame.Size.x <= 0.f || InFrame.Size.y <= 0.f)
        {
            return SpriteUVRect{};   // whole texture
        }

        // V is flipped: Offset.y counts from the top, GL samples from the bottom.
        const float lU0 = InFrame.Offset.x / lWidth;
        const float lU1 = (InFrame.Offset.x + InFrame.Size.x) / lWidth;
        const float lV0 = 1.f - (InFrame.Offset.y + InFrame.Size.y) / lHeight;
        const float lV1 = 1.f - InFrame.Offset.y / lHeight;

        return SpriteUVRect{ { lU0, lV0 }, { lU1, lV1 } };
    }

    TDynArray<SpriteFrame> SliceGrid(const SpriteSheetGrid& InGrid, const Uint32 InTexWidth, const Uint32 InTexHeight)
    {
        TDynArray<SpriteFrame> lFrames;

        const float lWidth  = static_cast<float>(InTexWidth);
        const float lHeight = static_cast<float>(InTexHeight);

        if (lWidth <= 0.f || lHeight <= 0.f || InGrid.CellSize.x <= 0.f || InGrid.CellSize.y <= 0.f)
        {
            return lFrames;
        }

        const Uint32 lFitCols = CellsThatFit(lWidth  - InGrid.Margin.x, InGrid.CellSize.x, InGrid.Spacing.x);
        const Uint32 lFitRows = CellsThatFit(lHeight - InGrid.Margin.y, InGrid.CellSize.y, InGrid.Spacing.y);

        const Uint32 lColumns = (InGrid.Columns > 0u) ? InGrid.Columns : lFitCols;
        const Uint32 lRows    = (InGrid.Rows    > 0u) ? InGrid.Rows    : lFitRows;

        lFrames.reserve(static_cast<size_t>(lColumns) * lRows);

        // Row-major, like frame indices everywhere.
        for (Uint32 lRow = 0; lRow < lRows; ++lRow)
        {
            for (Uint32 lCol = 0; lCol < lColumns; ++lCol)
            {
                const float lX = InGrid.Margin.x + static_cast<float>(lCol) * (InGrid.CellSize.x + InGrid.Spacing.x);
                const float lY = InGrid.Margin.y + static_cast<float>(lRow) * (InGrid.CellSize.y + InGrid.Spacing.y);

                // Drop cells that fall outside the texture.
                if (lX + InGrid.CellSize.x > lWidth || lY + InGrid.CellSize.y > lHeight)
                {
                    continue;
                }

                // Unnamed: a generated name per cell would intern a string for nothing.
                lFrames.emplace_back(SpriteFrame{ OpaaxStringID(), { lX, lY }, InGrid.CellSize });
            }
        }

        return lFrames;
    }
}
