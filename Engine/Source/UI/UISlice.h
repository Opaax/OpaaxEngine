#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "UI/UIMargin.h"
#include "UI/UIQuad.h"

namespace Opaax
{
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
     * Cuts every quad to InClip, with its UVs; quads fully outside are dropped (used by UIImage fill).
     */
    void ClipQuadsTo(TDynArray<UIQuad>& InOutQuads, const Bounds2D& InClip);

    /**
     * Maps the quads' 0..1 UVs into InUVMin..InUVMax (a sheet frame). Run after the slice, before the clip.
     */
    void MapQuadUVsInto(TDynArray<UIQuad>& InOutQuads, const Vector2F& InUVMin, const Vector2F& InUVMax) noexcept;
}
