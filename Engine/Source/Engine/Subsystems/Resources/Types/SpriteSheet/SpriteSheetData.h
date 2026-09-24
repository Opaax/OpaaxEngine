#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    struct TextureResource;   // only NAMED — TResourcePath never completes its parameter

    // =============================================================================
    // SpriteSheetData — a texture cut into named frames, as DATA.
    //
    //   The MapData / MapFile / MapResource stack, one layer over: this is the memory form,
    //   SpriteSheetFile is the `.opaaxsheet` reader/writer, SpriteSheetResource is the CResource
    //   adapter. Each knows only its neighbours.
    //
    //   THE FRAMES ARE THE TRUTH; the grid below only generated them. Slicing is an explicit act
    //   that replaces the list, so nothing recomputes a rect behind the author's back and an
    //   irregular packed atlas is describable at all.
    // =============================================================================

    /** One frame's rectangle inside the sheet's texture. */
    struct SpriteFrame
    {
        /**
         * What animation will look this frame up by, so it IDENTIFIES rather than carries text
         * (**I13**): four bytes and an integer compare, not a heap string per frame. Invalid is a
         * real state — an unnamed frame is addressed by its index — so readers gate on IsValid()
         * and never on ToString(), which answers "None".
         */
        OpaaxStringID Name;

        /** TOP-LEFT corner in texture PIXELS, the way an artist and every atlas tool count. */
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
     * The last slice settings. AUTHORING MEMORY, not the source of truth — it is kept so re-slicing
     * is repeatable, and it describes nothing about frames an author has since moved by hand.
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
     * A sheet: which image, which frames, and which frame a sprite that has no opinion shows.
     *
     * NO OPAAX_PROPERTIES — Frames is a TDynArray and no property drawer draws a list. The editor
     * folds over the GRID and over the SELECTED FRAME, both of which are reflected; the list itself
     * is the panel's own UI, which is what a list has to be to be reorderable.
     */
    struct SpriteSheetData
    {
        /** Asset-relative ("Textures/Hero.png") or a mount ("/Engine/…"), like every other reference. */
        TResourcePath<TextureResource> Texture;

        TDynArray<SpriteFrame> Frames;

        /** Shown by a sprite that names no frame of its own. Out of range answers nothing. */
        Uint32 DefaultFrame = 0;

        SpriteSheetGrid Grid;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpriteSheetData, Texture, Frames, DefaultFrame, Grid)

        Uint32 FrameCount() const noexcept { return static_cast<Uint32>(Frames.size()); }

        /**
         * The frame InIndex names, or nullptr.
         *
         * NEGATIVE MEANS "the sheet's own DefaultFrame" — the sentinel SpriteComponent::Frame uses,
         * resolved here so the renderer and the editor cannot disagree about what -1 shows. An index
         * past the end answers nullptr rather than clamping, so a caller can SAY SO before falling
         * back; silently drawing a different frame is the failure this codebase refuses.
         */
        const SpriteFrame* FrameAt(Int32 InIndex) const noexcept
        {
            const Int32 lIndex = (InIndex < 0) ? static_cast<Int32>(DefaultFrame) : InIndex;

            return (lIndex >= 0 && static_cast<Uint32>(lIndex) < FrameCount()) ? &Frames[lIndex] : nullptr;
        }
    };

    /** A sub-rectangle of a texture in UV space — what Renderer2D::DrawSprite takes. */
    struct SpriteUVRect
    {
        Vector2F UVMin = { 0.f, 0.f };
        Vector2F UVMax = { 1.f, 1.f };
    };

    /**
     * InFrame's pixel rect as UVs of a texture InTexWidth x InTexHeight.
     *
     * IT CONTAINS THE V FLIP, and that is the whole reason it is one named function: TextureResource
     * decodes bottom-up because GL samples that way (**I16**), while a frame's Offset.y is measured
     * from the TOP. Getting it backwards draws a plausible-looking WRONG frame rather than failing,
     * which is exactly the class of bug a pure function with a test exists to stop.
     *
     * Free and pure so the maths is testable with no GL context — MakeSortKey's shape. A zero
     * texture dimension or a zero-sized frame answers the whole texture, never a divide by zero.
     */
    OPAAX_API SpriteUVRect MakeFrameUV(const SpriteFrame& InFrame, Uint32 InTexWidth, Uint32 InTexHeight);

    /**
     * The frames a grid cuts out of a texture InTexWidth x InTexHeight, in ROW-MAJOR order —
     * left to right, top to bottom, which is how every sheet in the wild is laid out and therefore
     * what an index means.
     *
     * Also free and pure: slicing is the editor's most consequential button and it needs no UI to
     * be tested. Cells that would fall outside the texture are dropped, so a grid that does not
     * divide evenly yields the frames that fit rather than rectangles hanging off the edge.
     */
    OPAAX_API TDynArray<SpriteFrame> SliceGrid(const SpriteSheetGrid& InGrid, Uint32 InTexWidth, Uint32 InTexHeight);
}
