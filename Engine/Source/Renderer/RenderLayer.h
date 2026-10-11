#pragma once

#include "Core/EngineAPI.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ERenderLayer — generated from Renderer/RenderLayerList.h
    // =============================================================================
    /**
     * Draw-order band for 2D. Renderer2D sorts each pass by (Layer, OrderInLayer, texture) before
     * batching, so a higher band always draws on top. Add a band in RenderLayerList.h.
     * Ascending = back to front: Background behind, UI in front.
     */
    enum class ERenderLayer : Uint8
    {
        #define OPAAX_RENDER_LAYER(Name) Name,
        #include "Renderer/RenderLayerList.h"
        #undef OPAAX_RENDER_LAYER
        Count
    };

    /**
     * The band's label, for logs and dropdowns. Use ToStringID for lookups.
     */
    inline const char* ToString(ERenderLayer InLayer) noexcept
    {
        switch (InLayer)
        {
            #define OPAAX_RENDER_LAYER(Name) case ERenderLayer::Name: return #Name;
            #include "Renderer/RenderLayerList.h"
            #undef OPAAX_RENDER_LAYER

            default: return "Unknown";
        }
    }

    /**
     * The bands as data, for editor dropdowns. Written out by hand (a #include cannot go inside
     * OPAAX_ENUM_VALUES). Count is not a band.
     */
    template<>
    struct TEnumValues<ERenderLayer>
    {
        static constexpr ERenderLayer Values[] =
        {
            #define OPAAX_RENDER_LAYER(Name) ERenderLayer::Name,
            #include "Renderer/RenderLayerList.h"
            #undef OPAAX_RENDER_LAYER
        };
    };

    /** Name of each band. Index with static_cast<Uint8>(ERenderLayer). */
    inline const OpaaxStringID g_RenderLayerIDs[] =
    {
        #define OPAAX_RENDER_LAYER(Name) OPAAX_ID(#Name),
        #include "Renderer/RenderLayerList.h"
        #undef OPAAX_RENDER_LAYER
    };

    /** ERenderLayer -> OpaaxStringID. */
    inline const OpaaxStringID& ToStringID(ERenderLayer InLayer) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InLayer);
        return (lIdx < static_cast<Uint8>(ERenderLayer::Count)) ? g_RenderLayerIDs[lIdx] : g_RenderLayerIDs[0];
    }

    /** OpaaxStringID -> ERenderLayer (linear search). */
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
