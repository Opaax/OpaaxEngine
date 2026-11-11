#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // UILayer — where a submitted canvas draws among the others: a higher layer on top, equal layers
    //   in the order they were submitted. The game's canvases are on Game; the dev console on Console.
    // =============================================================================
    namespace UILayer
    {
        inline constexpr Int32 Game    = 0;
        inline constexpr Int32 Console = 1000;

        /**
         * Inserts InItem after every item of its layer or a lower one, so a list built by submits stays
         * sorted by layer and keeps the submit order within a layer. InGetLayer(item) gives the layer.
         */
        template<typename T, typename TGetLayer>
        void InsertByLayer(TDynArray<T>& InOutItems, T InItem, TGetLayer&& InGetLayer)
        {
            const Int32 lLayer = InGetLayer(InItem);

            Uint64 lAt = InOutItems.size();
            while (lAt > 0 && InGetLayer(InOutItems[lAt - 1]) > lLayer)
            {
                --lAt;
            }
            InOutItems.insert(InOutItems.begin() + static_cast<Int64>(lAt), Move(InItem));
        }
    }
}
