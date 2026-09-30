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
     * Packs draw order into one key: [Layer:hi][OrderInLayer:mid][texSlot:lo].
     * Bits 32..39 = Layer; bits 8..23 = OrderInLayer + 32768 (so negatives sort first);
     * bits 0..7 = texture slot.
     */
    //------------------------------------------------------------------------------
    FORCEINLINE constexpr Uint64 MakeSortKey(ERenderLayer InLayer, Int16 InOrderInLayer, Uint32 InTexSlot)
    {
        const Uint64 lLayer = static_cast<Uint64>(static_cast<Uint8>(InLayer));
        const Uint64 lOrder = static_cast<Uint64>(static_cast<Int32>(InOrderInLayer) + 32768); // [0, 65535]
        const Uint64 lSlot  = static_cast<Uint64>(InTexSlot & 0xFFu);
        return (lLayer << 32) | (lOrder << 8) | lSlot;
    }
}
