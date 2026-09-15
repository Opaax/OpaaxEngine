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
    // The rect → quads geometry, FREE AND PURE — the half of an image that is maths.
    //
    //   `ResolveRect` is the precedent: the layout rule lives outside the widget so it can be
    //   asserted with nothing else built. These two are the same shape one level down — a 9-slice
    //   is gated here against a texture SIZE, with no texture, no provider and no GL context.
    // =============================================================================

    /**
     * The quads InRect is covered by when InBorderPixels is left UNSTRETCHED — 9-slice (**UI20**).
     *
     * Only Bounds and the UVs are written; the caller owns Color and Texture, since the same
     * geometry serves a tinted image and a plain one.
     *
     * Three degeneracies, all handled here rather than by every caller:
     * - a zero-width row or column is SKIPPED, so a left/right-only border emits 3 quads, not 9
     *   with 6 empties;
     * - a rect SMALLER than its own borders shrinks them proportionally (Unity's rule) rather than
     *   letting the corners overlap and the middle invert — the UVs keep the authored border, so
     *   the corner art compresses instead of tearing;
     * - a texture with no size cannot say where its border is, so it degrades to ONE quad.
     */
    OPAAX_API void BuildSlicedQuads(const Bounds2D& InRect, const UIMargin& InBorderPixels,
                                    const Vector2F& InTextureSize, TDynArray<UIQuad>& OutQuads);

    /**
     * Cut every quad down to InClip, taking its UVs with it; one that falls entirely outside is
     * dropped.
     *
     * What `UIImage`'s fill is made of, and the reason a fill and a 9-slice COMPOSE instead of
     * excluding each other: the fill is a clip rect over whatever geometry the image emitted.
     */
    OPAAX_API void ClipQuadsTo(TDynArray<UIQuad>& InOutQuads, const Bounds2D& InClip);

    /**
     * Map every quad's 0..1 UVs into the InUVMin..InUVMax rect of a larger texture — a sheet
     * FRAME (**UI25**). Identity for the whole texture. Run after the slice and before the clip,
     * since the clip cuts UVs proportionally and does not care what range they are in.
     */
    OPAAX_API void MapQuadUVsInto(TDynArray<UIQuad>& InOutQuads, const Vector2F& InUVMin, const Vector2F& InUVMax) noexcept;
}
