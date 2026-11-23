// Suite: UILayer — lists kept by layer, a higher one on top, equal layers in the order they came: submitted
// canvases by an Int32, the game canvas's trees by a UILayerKey.
#include <doctest.h>

#include "Core/String/OpaaxString.hpp"
#include "UI/UILayer.h"
#include "UI/UILayerTable.h"

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

TEST_CASE("UILayer: IndexForLayer is after its layer's last item; ClampToLayer keeps an index inside its layer")
{
    TDynArray<Submitted> lItems;
    Submit(lItems, 'm', 100);
    Submit(lItems, 'M', 100);
    Submit(lItems, 'd', 500);
    const auto lLayerOf = [](const Submitted& InItem) { return InItem.Layer; };

    CHECK(UILayer::IndexForLayer(lItems, 0, lLayerOf) == 0u);
    CHECK(UILayer::IndexForLayer(lItems, 100, lLayerOf) == 2u);
    CHECK(UILayer::IndexForLayer(lItems, 300, lLayerOf) == 2u);
    CHECK(UILayer::IndexForLayer(lItems, 500, lLayerOf) == 3u);
    CHECK(UILayer::IndexForLayer(lItems, 900, lLayerOf) == 3u);

    CHECK(UILayer::ClampToLayer(lItems, 100, 0, lLayerOf) == 0u);    // in its layer: as asked
    CHECK(UILayer::ClampToLayer(lItems, 100, 1, lLayerOf) == 1u);
    CHECK(UILayer::ClampToLayer(lItems, 100, 3, lLayerOf) == 2u);    // not over the 500
    CHECK(UILayer::ClampToLayer(lItems, 500, 0, lLayerOf) == 2u);    // not under the 100s
    CHECK(UILayer::ClampToLayer(lItems, 500, 99, lLayerOf) == 3u);
    CHECK(UILayer::ClampToLayer(lItems, 300, 0, lLayerOf) == 2u);    // a layer with no item: between them

    const TDynArray<Submitted> lEmpty;
    CHECK(UILayer::ClampToLayer(lEmpty, 100, 5, lLayerOf) == 0u);
}

TEST_CASE("UILayer: the same functions keep a list by UILayerKey, a sub-layer inside its parent")
{
    TDynArray<UILayerSettings> lSettings = DefaultUILayers();
    lSettings.push_back(UILayerSettings{ OpaaxTag("Menu.Pause"), 10 });
    TDynArray<OpaaxString> lProblems;
    const UILayerTable     lTable = UILayerTable::Build(lSettings, lProblems);

    struct Tree
    {
        char     Name = '?';
        OpaaxTag Layer;
    };
    const auto lKeyOf = [&lTable](const Tree& InTree) { return lTable.KeyOf(InTree.Layer); };

    TDynArray<Tree> lTrees;
    UILayer::InsertByLayer(lTrees, Tree{ 'd', UILayerDebug() }, lKeyOf);
    UILayer::InsertByLayer(lTrees, Tree{ 'p', OpaaxTag("Menu.Pause") }, lKeyOf);
    UILayer::InsertByLayer(lTrees, Tree{ 'm', UILayerMenu() }, lKeyOf);
    UILayer::InsertByLayer(lTrees, Tree{ 'h', OpaaxTag() }, lKeyOf);   // no layer: at 0, with Hud
    UILayer::InsertByLayer(lTrees, Tree{ 'M', UILayerMenu() }, lKeyOf);

    OpaaxString lOrder;
    for (const Tree& lTree : lTrees) { lOrder += lTree.Name; }
    CHECK(lOrder == "hmMpd");
}
