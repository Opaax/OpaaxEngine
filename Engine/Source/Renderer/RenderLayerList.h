// RenderLayerList.h
//
// Single source of truth for render layers (coarse draw-order bands). Add a new
// layer by inserting one OPAAX_RENDER_LAYER(Name) line below — the consumer
// (RenderLayer.h) re-includes this file with different macro definitions to expand
// the list into both the ERenderLayer enum body and the parallel g_RenderLayerIDs
// string array.
//
// ORDER MATTERS: the enum value = the draw band, ascending = back-to-front (painter's
// algorithm). Background draws first (furthest back); UI draws last (front-most).
//
// Do NOT add #pragma once — this file is intentionally re-included.

OPAAX_RENDER_LAYER(Background)
OPAAX_RENDER_LAYER(Default)
OPAAX_RENDER_LAYER(Foreground)
// Debug overlays (DebugDraw lines) sit ABOVE all world geometry but BELOW UI: an overlay that a
// future 2D UI pass could hide would be useless, and one drawn over the UI would be noise.
OPAAX_RENDER_LAYER(Debug)
OPAAX_RENDER_LAYER(UI)
