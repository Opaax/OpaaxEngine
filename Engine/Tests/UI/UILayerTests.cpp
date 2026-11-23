// Suite: UILayer — lists kept by layer, a higher one on top, equal layers in the order they came: submitted
// canvases by an Int32, the game canvas's trees by a UILayerKey; and the canvas that keeps its trees so: drawn,
// hit and hovered as one tree, a higher layer on top.
#include <doctest.h>

#include "Core/String/OpaaxString.hpp"
#include "UI/Modifiers/UITweenModifier.h"
#include "UI/UICanvas.h"
#include "UI/UILayer.h"
#include "UI/UILayerTable.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIPanel.h"

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

// =============================================================================
// The game canvas: the root's trees kept in layer order (UICanvas::SetLayers, UIWidget::SetLayer)
// =============================================================================

namespace
{
    /** A pass-through full-screen root holding one 200x80 button, and the clicks it took. */
    struct LayeredTree
    {
        UIWidget* Root   = nullptr;
        UIButton* Button = nullptr;
        Uint32    Clicks = 0;
    };

    void AddTree(UICanvas& InCanvas, LayeredTree& OutTree, const char* InName, const OpaaxTag InLayer,
                 const Vector2F& InButtonAt = { 0.f, 0.f })
    {
        TStrongObjectPtr<UIPanel> lRoot = NewObject<UIPanel>();
        lRoot->Name           = InName;
        lRoot->bHitTestable   = false;
        lRoot->Rect.AnchorMin = { 0.f, 0.f };
        lRoot->Rect.AnchorMax = { 1.f, 1.f };
        lRoot->Rect.SizeDelta = { 0.f, 0.f };
        lRoot->SetLayer(InLayer);

        UIButton* const lButton = static_cast<UIButton*>(lRoot->AddChild(NewObject<UIButton>()));
        lButton->Name = OpaaxString(InName) + "Button";
        UIRect lRect;
        lRect.AnchoredPosition = InButtonAt;
        lRect.SizeDelta        = { 200.f, 80.f };
        lButton->SetRect(lRect);
        lButton->Brushes.FadeDuration = 0.f;
        lButton->OnClick.Add([&OutTree]() { ++OutTree.Clicks; });

        OutTree.Button = lButton;
        OutTree.Root   = InCanvas.Root().AddChild(Move(lRoot));
    }

    OpaaxString RootOrder(const UICanvas& InCanvas)
    {
        OpaaxString lOrder;
        for (const TStrongObjectPtr<UIWidget>& lTree : InCanvas.Root().GetChildren())
        {
            lOrder += lOrder.IsEmpty() ? lTree->Name : "," + lTree->Name;
        }
        return lOrder;
    }

    /** The defaults, and Menu.Pause at 10. */
    UILayerTable LayersWithPause(const Int32 InMenuOrder = 100)
    {
        TDynArray<UILayerSettings> lSettings = DefaultUILayers();
        lSettings[1].Order = InMenuOrder;
        lSettings.push_back(UILayerSettings{ OpaaxTag("Menu.Pause"), 10 });

        TDynArray<OpaaxString> lProblems;
        return UILayerTable::Build(lSettings, lProblems);
    }

    void Click(UICanvas& InCanvas, const Vector2F& InAt)
    {
        InCanvas.RoutePointer({ EUIPointerEventType::Move, InAt, EUIPointerButton::None });
        InCanvas.RoutePointer({ EUIPointerEventType::Down, InAt, EUIPointerButton::Primary });
        InCanvas.RoutePointer({ EUIPointerEventType::Up, InAt, EUIPointerButton::Primary });
    }

    /** Debug mounted first, then Menu. The debug button covers the menu button's centre. */
    struct TwoLayers
    {
        UICanvas    Canvas{ 1080.f };
        LayeredTree Debug;
        LayeredTree Menu;

        TwoLayers()
        {
            Canvas.SetTargetSize(1920, 1080);
            Canvas.SetLayers(LayersWithPause());
            AddTree(Canvas, Debug, "Debug", UILayerDebug(), { 0.f, 0.f });
            AddTree(Canvas, Menu, "Menu", UILayerMenu(), { 60.f, 0.f });
            Canvas.Update();
        }
    };
}

TEST_CASE("UILayer canvas: a tree mounted later on a lower layer still draws under")
{
    TwoLayers lF;

    CHECK(RootOrder(lF.Canvas) == "Menu,Debug");

    TDynArray<UIDrawItem> lItems;
    lF.Canvas.BuildDrawList(lItems);
    REQUIRE(lItems.size() == 2u);
    CHECK(lItems[0].Quad == &lF.Menu.Button->GetQuads()[0]);
    CHECK(lItems[1].Quad == &lF.Debug.Button->GetQuads()[0]);
    CHECK(lItems[0].Order < lItems[1].Order);
}

TEST_CASE("UILayer canvas: where both are, the higher layer takes the pointer; the button under it never hears it")
{
    TwoLayers lF;

    // Over both buttons: only the debug one.
    CHECK(lF.Canvas.HitTest({ 20.f, 0.f }) == lF.Debug.Button);
    Click(lF.Canvas, { 20.f, 0.f });
    CHECK(lF.Canvas.GetHovered() == lF.Debug.Button);
    CHECK(lF.Debug.Clicks == 1u);
    CHECK(lF.Menu.Clicks == 0u);
    CHECK(lF.Menu.Button->GetInteractionState() == EUIInteractionState::Normal);

    // The part of the menu button the debug one leaves uncovered is the menu's.
    Click(lF.Canvas, { 150.f, 0.f });
    CHECK(lF.Canvas.GetHovered() == lF.Menu.Button);
    CHECK(lF.Menu.Clicks == 1u);
    CHECK(lF.Debug.Clicks == 1u);
}

TEST_CASE("UILayer canvas: a hiding tree on top lets the pointer through to the layer under it")
{
    TwoLayers lF;
    lF.Debug.Root->AddModifier<UITweenModifier>().SetPlayOn(EUITweenPlay::Hide);

    lF.Debug.Root->Hide();
    REQUIRE(lF.Debug.Root->IsHiding());
    CHECK(lF.Debug.Root->bVisible);   // still drawn while it hides

    CHECK(lF.Canvas.HitTest({ 20.f, 0.f }) == lF.Menu.Button);
    Click(lF.Canvas, { 20.f, 0.f });
    CHECK(lF.Menu.Clicks == 1u);
    CHECK(lF.Debug.Clicks == 0u);
}

TEST_CASE("UILayer canvas: a sub-layer stays inside its layer; BringToFront stays inside its own")
{
    TwoLayers   lF;
    LayeredTree lPause;
    LayeredTree lOptions;
    AddTree(lF.Canvas, lPause, "Pause", OpaaxTag("Menu.Pause"));
    AddTree(lF.Canvas, lOptions, "Options", UILayerMenu());

    CHECK(RootOrder(lF.Canvas) == "Menu,Options,Pause,Debug");

    // The first Menu tree comes over the other Menu tree, not over Menu.Pause or Debug.
    lF.Menu.Root->BringToFront();
    CHECK(RootOrder(lF.Canvas) == "Options,Menu,Pause,Debug");

    // A tree that is already last of its layer stays.
    lF.Debug.Root->BringToFront();
    CHECK(RootOrder(lF.Canvas) == "Options,Menu,Pause,Debug");
}

TEST_CASE("UILayer canvas: SetLayer and SetLayers re-place the trees and keep the pointer state")
{
    TwoLayers lF;
    lF.Canvas.RoutePointer({ EUIPointerEventType::Move, { 20.f, 0.f }, EUIPointerButton::None });
    REQUIRE(lF.Canvas.GetHovered() == lF.Debug.Button);

    LayeredTree lPause;
    AddTree(lF.Canvas, lPause, "Pause", OpaaxTag("Menu.Pause"));
    CHECK(RootOrder(lF.Canvas) == "Menu,Pause,Debug");

    // Menu over Debug: its sub-layer goes with it.
    lF.Canvas.SetLayers(LayersWithPause(600));
    CHECK(RootOrder(lF.Canvas) == "Debug,Menu,Pause");
    CHECK(lF.Canvas.GetHovered() == lF.Debug.Button);   // nothing was taken out
    CHECK(lF.Canvas.HitTest({ 20.f, 0.f }) == lF.Menu.Button);

    // A tree moved to another layer goes last in it.
    lF.Debug.Root->SetLayer(UILayerLoading());
    CHECK(RootOrder(lF.Canvas) == "Menu,Pause,Debug");
    CHECK(lF.Debug.Root->GetLayer() == UILayerLoading());
}

TEST_CASE("UILayer canvas: an index asked of the root is kept inside the tree's layer")
{
    TwoLayers   lF;
    LayeredTree lHud;
    AddTree(lF.Canvas, lHud, "Hud", UILayerHud());
    CHECK(RootOrder(lF.Canvas) == "Hud,Menu,Debug");

    // Index 0 for a Debug tree: after the lower layers.
    TStrongObjectPtr<UIWidget> lLate = NewObject<UIPanel>();
    lLate->Name = "Late";
    lLate->SetLayer(UILayerDebug());
    lF.Canvas.Root().AddChild(Move(lLate), 0);
    CHECK(RootOrder(lF.Canvas) == "Hud,Menu,Late,Debug");

    // Past the end for a Hud tree: the end of Hud.
    TStrongObjectPtr<UIWidget> lFirst = NewObject<UIPanel>();
    lFirst->Name = "First";
    lF.Canvas.Root().AddChild(Move(lFirst), 99);   // no layer: at 0, with Hud
    CHECK(RootOrder(lF.Canvas) == "Hud,First,Menu,Late,Debug");
}

TEST_CASE("UILayer canvas: with no layers, trees keep the order they were added in")
{
    UICanvas lCanvas(1080.f);
    LayeredTree lA;
    LayeredTree lB;
    AddTree(lCanvas, lA, "A", UILayerDebug());
    AddTree(lCanvas, lB, "B", UILayerHud());
    CHECK(RootOrder(lCanvas) == "A,B");   // an empty table: every layer is level
}
