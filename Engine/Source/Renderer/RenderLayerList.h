// RenderLayerList.h — the list of render layers. Add one OPAAX_RENDER_LAYER(Name) line.
// RenderLayer.h includes this file several times (no #pragma once).
// Order = draw order: Background first (back), UI last (front).

OPAAX_RENDER_LAYER(Background)
OPAAX_RENDER_LAYER(Default)
OPAAX_RENDER_LAYER(Foreground)
// Debug overlays sit above the world but below the UI.
OPAAX_RENDER_LAYER(Debug)
OPAAX_RENDER_LAYER(UI)
