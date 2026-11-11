// Suite: UILayer — submitted canvases kept by layer, a higher one on top, equal layers in submit order.
#include <doctest.h>

#include "Core/String/OpaaxString.hpp"
#include "UI/UILayer.h"

using namespace Opaax;

namespace
{
    struct Submitted
    {
        char  Name  = '?';
        Int32 Layer = 0;
    };

    OpaaxString Order(const TDynArray<Submitted>& InItems)
    {
        OpaaxString lOrder;
        for (const Submitted& lItem : InItems)
        {
            lOrder += lItem.Name;
        }
        return lOrder;
    }

    void Submit(TDynArray<Submitted>& InOutItems, const char InName, const Int32 InLayer)
    {
        UILayer::InsertByLayer(InOutItems, Submitted{ InName, InLayer }, [](const Submitted& InItem) { return InItem.Layer; });
    }
}

TEST_CASE("UILayer: a canvas submitted first on a higher layer still draws last")
{
    TDynArray<Submitted> lItems;
    Submit(lItems, 'C', UILayer::Console);   // the console ticks before the UI
    Submit(lItems, 'H', UILayer::Game);      // the HUD
    Submit(lItems, 'L', UILayer::Game);      // the loading screen, over the HUD
    CHECK(Order(lItems) == "HLC");
}

TEST_CASE("UILayer: equal layers keep their submit order; a lower layer goes under")
{
    TDynArray<Submitted> lItems;
    Submit(lItems, 'a', 5);
    Submit(lItems, 'b', 5);
    Submit(lItems, 'c', -1);
    Submit(lItems, 'd', 5);
    Submit(lItems, 'e', 7);
    Submit(lItems, 'f', 6);
    CHECK(Order(lItems) == "cabdfe");
}
