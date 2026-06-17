#pragma once

#include "Core/EngineAPI.h"        // FORCEINLINE
#include "Core/OpaaxTypes.h"       // Uint64 / Uint32 / Int32 / Int16 / Uint8
#include "Renderer/RenderLayer.h"  // ERenderLayer

namespace Opaax
{
    // =============================================================================
    // Renderer2D draw-order sort key
    // =============================================================================

    /**
     * Pack draw order into one sortable key: [Layer:hi][OrderInLayer:mid][texSlot:lo].
     * OrderInLayer (Int16) is biased to unsigned so negative orders sort before positive.
     *
     * Bit layout (Uint64): bits 32..39 = Layer (Uint8); bits 8..23 = biased OrderInLayer
     * (Int16 + 32768 -> [0, 65535]); bits 0..7 = texture slot (low 8 bits).
     *
     * Hoisted out of Renderer2D.cpp so it is unit-testable in isolation. constexpr lets the
     * test evaluate it at compile time; the renderer hot path still inlines it.
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
