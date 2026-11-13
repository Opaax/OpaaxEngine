#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "UI/UIMargin.h"
#include "UI/UIQuad.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UISlice);

    /** The most quads a tiled image may take; past it the image is stretched instead. */
    inline constexpr Uint64 UI_MAX_TILED_QUADS = 1024;

    // =============================================================================
    // Rect -> quads geometry (pure functions, testable without GL).
    // =============================================================================

    /**
     * 9-slice: the quads covering InRect with InBorderPixels not stretched.
     * Only Bounds and UVs are written (the caller sets Color and Texture).
     *   - zero-width rows/columns are skipped (a left/right-only border gives 3 quads);
     *   - a rect smaller than its borders shrinks them proportionally (UVs keep the border);
     *   - a texture without size gives one quad.
     */
    void BuildSlicedQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels,
                                    const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads);

    /**
     * 9-slice at a scale: a border pixel takes InBorderScale canvas units (zero or less reads as 1).
     * The UVs still cover InBorderPixels; only the corners' and edges' size on screen changes.
     */
    void BuildSlicedQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels, float InBorderScale,
                          const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads);

    /**
     * 9-slice with the edges and the centre repeated instead of stretched. A tile is its source
     * pixels times InBorderScale; tiles start at the left and bottom edges and the last one of a run
     * is cut, its UVs with it. Corners are as BuildSlicedQuads makes them. A zero border repeats the
     * whole texture both ways; an edge or centre with no source pixels is stretched.
     * @return False, appending nothing, when it would take more than UI_MAX_TILED_QUADS quads
     */
    bool BuildTiledQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels, float InBorderScale,
                         const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads);

    /** InBorderScale as a slice uses it: zero or less (or not a number) is 1. */
    float SanitizeBorderScale(float InBorderScale) noexcept;

    /**
     * Cuts every quad to InClip, with its UVs; quads fully outside are dropped (used by UIImage fill).
     */
    void ClipQuadsTo(TDynArray<UIQuad>& InOutQuads, const Bounds2D& InClip);

    /**
     * Maps the quads' 0..1 UVs into InUVMin..InUVMax (a sheet frame), which becomes their UVFrame. Run after the slice, before the clip.
     */
    void MapQuadUVsInto(TDynArray<UIQuad>& InOutQuads, const Vector2F& InUVMin, const Vector2F& InUVMax) noexcept;
}
