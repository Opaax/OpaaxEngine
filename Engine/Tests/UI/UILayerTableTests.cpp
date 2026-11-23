// Suite: UILayerTable — the game canvas's layers from a list of named orders (UI.Layers): keys, sub-layers,
// unlisted names and what is wrong with a list.
#include <doctest.h>

#include "UI/UILayerTable.h"

using namespace Opaax;

namespace
{
    UILayerTable BuildTable(const TDynArray<UILayerSettings>& InSettings)
    {
        TDynArray<OpaaxString> lProblems;
        UILayerTable           lTable = UILayerTable::Build(InSettings, lProblems);
        CHECK(lProblems.empty());
        return lTable;
    }

    /** The defaults with Menu.Pause at InPauseOrder. */
    UILayerTable WithPause(const Int32 InPauseOrder)
    {
        TDynArray<UILayerSettings> lSettings = DefaultUILayers();
        lSettings.push_back(UILayerSettings{ OpaaxTag("Menu.Pause"), InPauseOrder });
        return BuildTable(lSettings);
    }

    /** The listed layers bottom to top, each under UI.Layer, written without that root. */
    OpaaxString Names(const UILayerTable& InTable)
    {
        const OpaaxString lRoot = UILayerRoot().ToString() + ".";

        OpaaxString lNames;
        for (const UILayerTable::Entry& lEntry : InTable.GetLayers())
        {
            const OpaaxString lName = lEntry.Name.ToString();
            CHECK(lName.StartsWith(lRoot));
            const OpaaxString lShort = lName.SubString(lRoot.GetLength());
            lNames += lNames.IsEmpty() ? lShort : "," + lShort;
        }
        return lNames;
    }
}

TEST_CASE("UILayerTable: the defaults are Hud, Menu, Debug, Loading from the bottom")
{
    const UILayerTable lTable = BuildTable(DefaultUILayers());

    CHECK(Names(lTable) == "Hud,Menu,Debug,Loading");
    CHECK(lTable.KeyOf(UILayerHud()) < lTable.KeyOf(UILayerMenu()));
    CHECK(lTable.KeyOf(UILayerMenu()) < lTable.KeyOf(UILayerDebug()));
    CHECK(lTable.KeyOf(UILayerDebug()) < lTable.KeyOf(UILayerLoading()));
    CHECK(lTable.KeyOf(UILayerMenu()).ToString() == "100");
}

TEST_CASE("UILayerTable: a list in any order sorts by order, equal orders keep the list's order")
{
    const UILayerTable lTable = BuildTable({ UILayerSettings{ OpaaxTag("Top"), 7 },
                                             UILayerSettings{ OpaaxTag("A"), 5 },
                                             UILayerSettings{ OpaaxTag("Under"), -1 },
                                             UILayerSettings{ OpaaxTag("B"), 5 } });

    CHECK(Names(lTable) == "Under,A,B,Top");
    CHECK(lTable.KeyOf(OpaaxTag("A")) == lTable.KeyOf(OpaaxTag("B")));
}

TEST_CASE("UILayerTable: a sub-layer orders among its parent's own trees and never leaves its parent's place")
{
    const UILayerKey lMenu  = BuildTable(DefaultUILayers()).KeyOf(UILayerMenu());
    const UILayerKey lDebug = BuildTable(DefaultUILayers()).KeyOf(UILayerDebug());

    SUBCASE("over the parent's own trees")
    {
        const UILayerTable lTable = WithPause(10);
        const UILayerKey   lPause = lTable.KeyOf(OpaaxTag("Menu.Pause"));
        CHECK(lPause.ToString() == "100/10");
        CHECK(lMenu < lPause);
        CHECK(lPause < lDebug);
        CHECK(Names(lTable) == "Hud,Menu,Menu.Pause,Debug,Loading");
    }

    SUBCASE("under them with a negative order, still over the layer below")
    {
        const UILayerKey lPause = WithPause(-10).KeyOf(OpaaxTag("Menu.Pause"));
        CHECK(lPause < lMenu);
        CHECK(BuildTable(DefaultUILayers()).KeyOf(UILayerHud()) < lPause);
    }

    SUBCASE("a high order stays under the next layer up")
    {
        const UILayerKey lPause = WithPause(9999).KeyOf(OpaaxTag("Menu.Pause"));
        CHECK(lMenu < lPause);
        CHECK(lPause < lDebug);
    }

    SUBCASE("at 0, level with the parent's own trees")
    {
        CHECK(WithPause(0).KeyOf(OpaaxTag("Menu.Pause")) == lMenu);
    }
}

TEST_CASE("UILayerTable: an unlisted name draws in its nearest listed parent; none at all is 0")
{
    const UILayerTable lTable = WithPause(10);

    CHECK(lTable.Resolve(OpaaxTag("Menu.Pause.Extra")) == OpaaxTag("UI.Layer.Menu.Pause"));   // a short name, read under the root
    CHECK(lTable.Resolve(OpaaxTag("UI.Layer.Menu.Pause.Extra")) == OpaaxTag("UI.Layer.Menu.Pause"));
    CHECK(lTable.KeyOf(OpaaxTag("Menu.Pause.Extra")) == lTable.KeyOf(OpaaxTag("Menu.Pause")));
    CHECK(lTable.Resolve(OpaaxTag("Menu.Options")) == UILayerMenu());

    CHECK_FALSE(lTable.Resolve(OpaaxTag("Foo")).IsValid());
    CHECK(lTable.KeyOf(OpaaxTag("Foo")) == UILayerKey{});
    CHECK(lTable.KeyOf(OpaaxTag()) == UILayerKey{});
    CHECK(UILayerKey{}.ToString() == "0");

    CHECK(lTable.IsListed(OpaaxTag("Menu.Pause")));
    CHECK_FALSE(lTable.IsListed(OpaaxTag("Menu.Options")));

    // An empty table: everything is level, at 0.
    const UILayerTable lEmpty;
    CHECK(lEmpty.KeyOf(UILayerDebug()) == lEmpty.KeyOf(UILayerHud()));
}

TEST_CASE("UILayerTable: what is wrong with a list is reported, one line each, and worked around")
{
    TDynArray<OpaaxString> lProblems;
    const UILayerTable     lTable = UILayerTable::Build({ UILayerSettings{ OpaaxTag(), 3 },
                                                          UILayerSettings{ OpaaxTag("Menu"), 100 },
                                                          UILayerSettings{ OpaaxTag("Menu"), 200 },
                                                          UILayerSettings{ OpaaxTag("Popup.Toast"), 5 },
                                                          UILayerSettings{ OpaaxTag("A.B.C.D.E"), 1 } },
                                                        lProblems);

    REQUIRE(lProblems.size() == 4);
    CHECK(lProblems[0].Contains("no name"));
    CHECK(lProblems[1].Contains("'UI.Layer.Menu' is listed twice"));
    CHECK(lProblems[2].Contains("'UI.Layer.A.B.C.D.E' is more than 4 names under UI.Layer"));
    CHECK(lProblems[3].Contains("'UI.Layer.Popup.Toast' is under 'UI.Layer.Popup', which is not listed"));

    CHECK(Names(lTable) == "Popup.Toast,Menu");                    // {0, 5} under {100}
    CHECK(lTable.KeyOf(UILayerMenu()).ToString() == "100");         // the first Menu kept
    CHECK(lTable.KeyOf(OpaaxTag("Popup.Toast")).ToString() == "0/5");
}

TEST_CASE("UILayerTable: a sub-layer listed before its parent reads the parent's order")
{
    const UILayerTable lTable = BuildTable({ UILayerSettings{ OpaaxTag("Menu.Pause"), 10 },
                                             UILayerSettings{ OpaaxTag("Menu"), 100 } });

    CHECK(lTable.KeyOf(OpaaxTag("Menu.Pause")).ToString() == "100/10");
    CHECK(Names(lTable) == "Menu,Menu.Pause");
}
