#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "Renderer/Text/DrawParams.h"

namespace Opaax
{
    class FontAsset;
}

namespace Opaax::Text2D
{
    // =============================================================================
    // Text2D
    //
    // Engine-level text rendering primitive. World-space, top-left-anchored.
    // Issues one Renderer2D::DrawSprite call per visible glyph into the existing
    // batched-quad path. Position is the top-left of the first glyph's bounding
    // box; the layout walker advances right + downward (Y-up world: downward =
    // smaller Y). Screen-space text is faked by callers updating worldPos per
    // frame against the camera (see future HUD milestone for ergonomic anchoring).
    //
    // Out-of-scope for M5 (locked at D-c plan review):
    //   - Anchor (top-left only)
    //   - Rotation
    //   - Word-wrap
    //   - Outline / shadow
    //   - Unicode / UTF-8 (ASCII printable [0x20..0x7E] only per D-f)
    // =============================================================================

    /**
     * Draw a single line or multi-line string at world InWorldPos using InFont.
     * '\n' triggers a line break (LineHeightScale * LineAdvance * Scale downward).
     * '\t' renders as 4 spaces. Codepoints outside [0x20..0x7E] render '?' as
     * fallback per OD-3 lock. No-op if InText is null/empty or InFont isn't loaded.
     */
    OPAAX_API void DrawString(const char*       InText,
                              const Vector2F&   InWorldPos,
                              const FontAsset&  InFont,
                              const DrawParams& InParams = {});

    /**
     * Compute the world-space bounding-box size the same string would occupy
     * under InParams without drawing. Returns the {max line width, total height}
     * — total height = N_lines * LineAdvance * LineHeightScale * Scale where
     * N_lines counts '\n'-separated segments (1 minimum). Exposed for future
     * Draw Debug API consumers that center text under an entity.
     */
    OPAAX_API Vector2F Measure(const char*       InText,
                               const FontAsset&  InFont,
                               const DrawParams& InParams = {});

} // namespace Opaax::Text2D
