#pragma once

#include "Core/EngineAPI.h"        // FORCEINLINE
#include "Core/OpaaxTypes.h"
#include "Renderer/RenderLayer.h"  // ERenderLayer

namespace Opaax
{
    // =============================================================================
    // Renderer2D draw-order sort key
    // =============================================================================

    /**
     * Packs draw order into one key: [Layer][OrderInLayer][material][texture].
     * Bits 40..47 = Layer; bits 24..39 = OrderInLayer + 32768 (so negatives sort first);
     * bits 8..23 = material (0: the engine's sprite shader); bits 0..7 = texture id.
     * The material and the texture only group quads of one layer and order: what is drawn on top
     * never depends on them.
     */
    //------------------------------------------------------------------------------
    FORCEINLINE constexpr Uint64 MakeSortKey(ERenderLayer InLayer, Int16 InOrderInLayer, Uint32 InTexSlot,
                                             Uint32 InMaterial = 0u)
    {
        const Uint64 lLayer    = static_cast<Uint64>(static_cast<Uint8>(InLayer));
        const Uint64 lOrder    = static_cast<Uint64>(static_cast<Int32>(InOrderInLayer) + 32768); // [0, 65535]
        const Uint64 lMaterial = static_cast<Uint64>(InMaterial & 0xFFFFu);
        const Uint64 lSlot     = static_cast<Uint64>(InTexSlot & 0xFFu);
        return (lLayer << 40) | (lOrder << 24) | (lMaterial << 8) | lSlot;
    }
}
