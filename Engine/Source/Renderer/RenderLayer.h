#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ERenderLayer — X-Macro driven (Renderer/RenderLayerList.h)
    // =============================================================================
    /**
     * @enum ERenderLayer
     * Coarse draw-order band for 2D draws. Renderer2D sorts the batch by
     * (Layer, OrderInLayer, textureSlot) before flushing, so a higher band always
     * draws on top regardless of submission order. The enum body and the matching
     * g_RenderLayerIDs[] are both generated from RenderLayerList.h — adding a band
     * means a single new line in that list file.
     *
     * Ascending value = back-to-front: Background behind, UI in front.
     */
    enum class ERenderLayer : Uint8
    {
        #define OPAAX_RENDER_LAYER(Name) Name,
        #include "RenderLayerList.h"
        #undef OPAAX_RENDER_LAYER
        Count
    };

    /*** Parallel canonical-name array. Index by static_cast<Uint8>(ERenderLayer). */
    inline const OpaaxStringID g_RenderLayerIDs[] =
    {
        #define OPAAX_RENDER_LAYER(Name) OPAAX_ID(#Name),
        #include "RenderLayerList.h"
        #undef OPAAX_RENDER_LAYER
    };

    /*** ERenderLayer -> canonical OpaaxStringID. O(1) LUT. */
    inline const OpaaxStringID& ToStringID(ERenderLayer InLayer) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InLayer);
        return (lIdx < static_cast<Uint8>(ERenderLayer::Count)) ? g_RenderLayerIDs[lIdx] : g_RenderLayerIDs[0];
    }

    /*** OpaaxStringID -> ERenderLayer. Linear scan; pure integer compare per slot. */
    inline ERenderLayer RenderLayerFromStringID(const OpaaxStringID& InID) noexcept
    {
        for (Uint8 i = 0; i < static_cast<Uint8>(ERenderLayer::Count); ++i)
        {
            if (g_RenderLayerIDs[i] == InID)
            {
                return static_cast<ERenderLayer>(i);
            }
        }
        return ERenderLayer::Default;
    }
}
