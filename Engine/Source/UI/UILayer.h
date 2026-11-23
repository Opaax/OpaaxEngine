#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // UILayer — keeping a list in layer order: a higher layer on top, equal layers in the order they
    //   came. Submitted canvases are kept by an Int32 layer: the game's canvases on Game, the dev
    //   console on Console. The trees of the game's canvas are kept by a UILayerKey (UI.Layers in the
    //   engine config, UI/UILayerTable.h). A layer only needs operator<.
    // =============================================================================
    namespace UILayer
    {
        inline constexpr Int32 Game    = 0;
        inline constexpr Int32 Console = 1000;

        /**
         * Where an item of InLayer goes in InItems (kept in layer order): after every item of its layer
         * or a lower one. InGetLayer(item) gives an item's layer.
         */
        template<typename T, typename TLayer, typename TGetLayer>
        Uint64 IndexForLayer(const TDynArray<T>& InItems, const TLayer& InLayer, TGetLayer&& InGetLayer)
        {
            Uint64 lAt = InItems.size();
            while (lAt > 0 && InLayer < InGetLayer(InItems[lAt - 1]))
            {
                --lAt;
            }
            return lAt;
        }

        /**
         * InIndex kept where an item of InLayer may go in InItems (kept in layer order): not under an
         * item of a lower layer, not over one of a higher layer.
         */
        template<typename T, typename TLayer, typename TGetLayer>
        Uint64 ClampToLayer(const TDynArray<T>& InItems, const TLayer& InLayer, const Uint64 InIndex,
                            TGetLayer&& InGetLayer)
        {
            Uint64 lFirst = 0;
            while (lFirst < InItems.size() && InGetLayer(InItems[lFirst]) < InLayer)
            {
                ++lFirst;
            }

            const Uint64 lLast = IndexForLayer(InItems, InLayer, InGetLayer);
            return InIndex < lFirst ? lFirst : (InIndex > lLast ? lLast : InIndex);
        }

        /**
         * Inserts InItem after every item of its layer or a lower one, so a list built by inserts stays
         * in layer order and keeps the insert order within a layer. InGetLayer(item) gives the layer.
         */
        template<typename T, typename TGetLayer>
        void InsertByLayer(TDynArray<T>& InOutItems, T InItem, TGetLayer&& InGetLayer)
        {
            const auto   lLayer = InGetLayer(InItem);
            const Uint64 lAt    = IndexForLayer(InOutItems, lLayer, InGetLayer);
            InOutItems.insert(InOutItems.begin() + static_cast<Int64>(lAt), Move(InItem));
        }
    }
}
