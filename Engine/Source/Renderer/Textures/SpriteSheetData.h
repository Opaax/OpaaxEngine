#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct TextureResource;

    // =============================================================================
    // SpriteSheetData — a texture cut into named frames (.opaaxsheet).
    //   The frame list is the truth; the grid settings only generated it.
    // =============================================================================

    /** One frame's rectangle in the sheet's texture. */
    struct SpriteFrame
    {
        /**
         * Name used by animations. May be invalid (the frame is then used by index).
         */
        OpaaxStringID Name;

        /** Top-left corner, in texture pixels. */
        Vector2F Offset = { 0.f, 0.f };

        /** Width and height in pixels. */
        Vector2F Size = { 0.f, 0.f };

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpriteFrame, Name, Offset, Size)

        OPAAX_PROPERTIES(SpriteFrame,
                         OPAAX_PROP(Name).SetTooltip("What animation looks this frame up by. Empty is\n"
                                                     "fine — the frame is then addressed by its index."),
                         OPAAX_PROP(Offset).SetTooltip("Top-left corner in texture pixels."),
                         OPAAX_PROP(Size).SetTooltip("Width and height in texture pixels."))
    };

    /**
     * The last slice settings, kept so slicing again gives the same result.
     */
    struct SpriteSheetGrid
    {
        Vector2F CellSize = { 32.f, 32.f };
        Vector2F Margin   = { 0.f, 0.f };    // border skipped on the top-left
        Vector2F Spacing  = { 0.f, 0.f };    // gap between cells
        Uint32   Columns  = 0;               // 0 = as many as the texture fits
        Uint32   Rows     = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpriteSheetGrid, CellSize, Margin, Spacing, Columns, Rows)

        OPAAX_PROPERTIES(SpriteSheetGrid,
                         OPAAX_PROP(CellSize).SetRange(1.f, 4096.f).SetTooltip("One cell's size in pixels."),
                         OPAAX_PROP(Margin).SetRange(0.f, 4096.f).SetTooltip("Border skipped at the top-left of the texture."),
                         OPAAX_PROP(Spacing).SetRange(0.f, 4096.f).SetTooltip("Gap between two cells."),
                         OPAAX_PROP(Columns).SetRange(0.f, 512.f).SetTooltip("0 fits as many as the texture allows."),
                         OPAAX_PROP(Rows).SetRange(0.f, 512.f).SetTooltip("0 fits as many as the texture allows."))
    };

    /**
     * A sheet: its texture, its frames, and the default frame.
     * Frames are edited by the editor panel (lists are not drawn by the property system).
     */
    struct SpriteSheetData
    {
        /** Asset-relative ("Textures/Hero.png") or a mount ("/Engine/..."). */
        TResourcePath<TextureResource> Texture;

        TDynArray<SpriteFrame> Frames;

        /** Shown by a sprite that names no frame. */
        Uint32 DefaultFrame = 0;

        SpriteSheetGrid Grid;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpriteSheetData, Texture, Frames, DefaultFrame, Grid)

        Uint32 FrameCount() const noexcept { return static_cast<Uint32>(Frames.size()); }

        /**
         * The frame at InIndex, or nullptr. Negative means the DefaultFrame.
         * Out of range gives nullptr (no clamping).
         */
        const SpriteFrame* FrameAt(Int32 InIndex) const noexcept
        {
            const Int32 lIndex = (InIndex < 0) ? static_cast<Int32>(DefaultFrame) : InIndex;

            return (lIndex >= 0 && static_cast<Uint32>(lIndex) < FrameCount()) ? &Frames[lIndex] : nullptr;
        }
    };

    /** A texture sub-rectangle in UV space (for Renderer2D::DrawSprite). */
    struct SpriteUVRect
    {
        Vector2F UVMin = { 0.f, 0.f };
        Vector2F UVMax = { 1.f, 1.f };
    };

    /**
     * InFrame's pixel rect as UVs of a texture InTexWidth x InTexHeight. Handles the V flip
     * (textures are bottom-up for GL). A zero size gives the whole texture.
     */
    SpriteUVRect MakeFrameUV(const SpriteFrame& InFrame, Uint32 InTexWidth, Uint32 InTexHeight);

    /**
     * The frames a grid cuts out of a texture, in row-major order (left to right, top to bottom).
     * Cells outside the texture are dropped.
     */
    TDynArray<SpriteFrame> SliceGrid(const SpriteSheetGrid& InGrid, Uint32 InTexWidth, Uint32 InTexHeight);
}
