// Suite: UI bindings (UI/UIBinding.h) — UI24.
//
// A widget pulls a named value from a reflected object the game owns. What is gated: the reader
// answers the four readable kinds by name and refuses the rest; the canvas's table resolves
// "Source.Property" and warns ONCE; a bound text rebuilds when the value changes and NOT when it
// holds (the per-frame pull must leave an idle canvas at 0/0); the {} format; the fill binding;
// and both fields surviving the file.
#include <doctest.h>

#include "Core/Maths/MathTypes.h"
#include "UI/UIBinding.h"
#include "UI/UICanvas.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidgetRegistry.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace
{
    /** A game's view model: every readable kind, and one that is not. */
    struct Model
    {
        Uint32      Lives    = 3;
        float       Health   = 0.5f;
        bool        bArmed   = true;
        OpaaxString Name     = "Ann";
        Vector2F    Position = { 1.f, 2.f };

        OPAAX_PROPERTIES(Model,
                         OPAAX_PROP(Lives),
                         OPAAX_PROP(Health),
                         OPAAX_PROP(bArmed),
                         OPAAX_PROP(Name),
                         OPAAX_PROP(Position))
    };
}

TEST_CASE("UIBinding: the reader answers int, float, bool and string by name, and refuses a Vector")
{
    Model lModel;
    const UIBindingReader lReader = MakeBindingReader(lModel);

    UIBoundValue lValue;
    REQUIRE(lReader("Lives", lValue));
    CHECK(lValue.Kind == UIBoundValue::EKind::Integer);
    CHECK(lValue.ToText() == OpaaxString("3"));
    CHECK(lValue.ToFloat() == doctest::Approx(3.f));

    REQUIRE(lReader("Health", lValue));
    CHECK(lValue.Kind == UIBoundValue::EKind::Number);
    CHECK(lValue.ToText() == OpaaxString("0.5"));

    REQUIRE(lReader("bArmed", lValue));
    CHECK(lValue.Kind == UIBoundValue::EKind::Bool);
    CHECK(lValue.ToText() == OpaaxString("true"));
    CHECK(lValue.ToFloat() == doctest::Approx(1.f));

    REQUIRE(lReader("Name", lValue));
    CHECK(lValue.Kind == UIBoundValue::EKind::Text);
    CHECK(lValue.ToText() == OpaaxString("Ann"));

    CHECK_FALSE(lReader("Position", lValue));   // a Vector is not a value a widget can show
    CHECK_FALSE(lReader("Mana", lValue));       // no such field

    // BORROWED: the reader sees the live object, not a copy taken at MakeBindingReader.
    lModel.Lives = 7;
    REQUIRE(lReader("Lives", lValue));
    CHECK(lValue.ToText() == OpaaxString("7"));
}

TEST_CASE("UIBinding: FormatBoundText replaces the first {} — or the whole text when there is none")
{
    CHECK(FormatBoundText(OpaaxString("Jumps: {}"), OpaaxString("4"))     == OpaaxString("Jumps: 4"));
    CHECK(FormatBoundText(OpaaxString("{} / {}"),   OpaaxString("4"))     == OpaaxString("4 / {}"));
    CHECK(FormatBoundText(OpaaxString("Score"),     OpaaxString("4"))     == OpaaxString("4"));
    CHECK(FormatBoundText(OpaaxString(""),          OpaaxString("4"))     == OpaaxString("4"));
    CHECK(FormatBoundText(OpaaxString("{}"),        OpaaxString("Ann"))   == OpaaxString("Ann"));
}

TEST_CASE("UIBinding: the table resolves Source.Property, and an unknown path fails without throwing")
{
    Model lModel;
    UIBindingTable lTable;
    CHECK(lTable.Count() == 0u);

    const UIBindingHandle lFirst = lTable.Add(OPAAX_ID("Player"), MakeBindingReader(lModel));
    CHECK(lFirst.IsValid());
    CHECK(lTable.Count() == 1u);
    CHECK(lTable.Has(OPAAX_ID("Player")));

    UIBoundValue lValue;
    CHECK(lTable.Read(OpaaxString("Player.Lives"), lValue));
    CHECK(lValue.ToText() == OpaaxString("3"));

    CHECK_FALSE(lTable.Read(OpaaxString("Player.Mana"), lValue));       // no such property
    CHECK_FALSE(lTable.Read(OpaaxString("Player.Mana"), lValue));       // the second time is silent — not asserted, read the log
    CHECK_FALSE(lTable.Read(OpaaxString("Enemy.Lives"), lValue));       // no such source
    CHECK_FALSE(lTable.Read(OpaaxString("Player"), lValue));            // no dot
    CHECK_FALSE(lTable.Read(OpaaxString(""), lValue));

    // A second Add under the same name REPLACES — the LEVEL SWAP: the next world's HUD registers
    // "Player" while the old world is still alive.
    Model lOther;
    lOther.Lives = 9;
    const UIBindingHandle lSecond = lTable.Add(OPAAX_ID("Player"), MakeBindingReader(lOther));
    CHECK(lSecond.IsValid());
    CHECK(lSecond.Ticket != lFirst.Ticket);
    CHECK(lTable.Count() == 1u);
    CHECK(lTable.Read(OpaaxString("Player.Lives"), lValue));
    CHECK(lValue.ToText() == OpaaxString("9"));

    // THEN the old world shuts down and removes ITS source: the new one must survive that. This
    // is the bug their eyes found — after a swap the HUD showed its authored "Jumps: {}" because
    // the old HUD's remove-by-name had taken the new HUD's source with it.
    lTable.Remove(lFirst);
    CHECK(lTable.Count() == 1u);
    CHECK(lTable.Has(OPAAX_ID("Player")));
    CHECK(lTable.Read(OpaaxString("Player.Lives"), lValue));
    CHECK(lValue.ToText() == OpaaxString("9"));

    lTable.Remove(lSecond);
    CHECK(lTable.Count() == 0u);
    CHECK_FALSE(lTable.Has(OPAAX_ID("Player")));

    lTable.Remove(lSecond);   // twice is nothing
    lTable.Remove(UIBindingHandle{});
    CHECK(lTable.Count() == 0u);
}

TEST_CASE("UIBinding: a bound UIText rebuilds when the value changes and not when it holds; unbound it shows its Text")
{
    // No font provider in the context, so Rebuild emits nothing — the REBUILD COUNT is the gate,
    // and GetDisplayText says what it would have laid out.
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    lCanvas.Update();   // the root's own first build, out of the numbers below

    Model lModel;
    const UIBindingHandle lSource = lCanvas.Bindings().Add(OPAAX_ID("Player"), MakeBindingReader(lModel));

    auto lOwned = MakeUnique<UIText>();
    lOwned->Text    = "Lives: {}";
    lOwned->Binding = "Player.Lives";
    UIText* lText = static_cast<UIText*>(lCanvas.Root().AddChild(Move(lOwned)));

    UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Rebuilds == 1);   // its first build
    CHECK(lText->GetDisplayText() == OpaaxString("Lives: 3"));
    CHECK(lText->Text == OpaaxString("Lives: {}"));   // the FORMAT is untouched — what a Save writes

    lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 0);   // the value held: the pull cost nothing
    CHECK(lStats.Rebuilds == 0);

    lModel.Lives = 2;
    lStats = lCanvas.Update();
    CHECK(lStats.Layouts  == 0);
    CHECK(lStats.Rebuilds == 1);
    CHECK(lText->GetDisplayText() == OpaaxString("Lives: 2"));

    // A path nothing answers: the authored text stands, and nothing rebuilds for it.
    auto lLost = MakeUnique<UIText>();
    lLost->Text    = "Mana: {}";
    lLost->Binding = "Player.Mana";
    UIText* lLostText = static_cast<UIText*>(lCanvas.Root().AddChild(Move(lLost)));
    lCanvas.Update();
    CHECK(lLostText->GetDisplayText() == OpaaxString("Mana: {}"));
    lStats = lCanvas.Update();
    CHECK(lStats.Rebuilds == 0);

    // The source leaves (a world ended): the last value stays on screen, nothing rebuilds.
    lCanvas.Bindings().Remove(lSource);
    lStats = lCanvas.Update();
    CHECK(lStats.Rebuilds == 0);
    CHECK(lText->GetDisplayText() == OpaaxString("Lives: 2"));
}

TEST_CASE("UIBinding: a UIImage's FillBinding drives its FillAmount")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    Model lModel;
    lCanvas.Bindings().Add(OPAAX_ID("Player"), MakeBindingReader(lModel));

    auto lOwned = MakeUnique<UIImage>();
    lOwned->Fill        = EUIFill::Horizontal;
    lOwned->FillAmount  = 1.f;
    lOwned->FillBinding = "Player.Health";
    UIImage* lBar = static_cast<UIImage*>(lCanvas.Root().AddChild(Move(lOwned)));

    lCanvas.Update();
    CHECK(lBar->FillAmount == doctest::Approx(0.5f));
    REQUIRE(lBar->GetQuads().size() == 1u);
    CHECK(lBar->GetQuads()[0].Bounds.Size().x == doctest::Approx(50.f));   // half of the default 100

    UICanvasStats lStats = lCanvas.Update();
    CHECK(lStats.Rebuilds == 0);

    lModel.Health = 0.25f;
    lStats = lCanvas.Update();
    CHECK(lStats.Rebuilds == 1);
    CHECK(lBar->GetQuads()[0].Bounds.Size().x == doctest::Approx(25.f));
}

TEST_CASE("UIBinding: both binding fields round-trip through the file, and a file without them reads")
{
    UIWidgetRegistry lRegistry;
    lRegistry.Register<UIPanel>(OPAAX_ID("UIPanel"));
    lRegistry.Register<UIText>(OPAAX_ID("UIText"));
    lRegistry.Register<UIImage>(OPAAX_ID("UIImage"));

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = MakeUnique<UIPanel>();

    auto lText = MakeUnique<UIText>();
    lText->Text    = "Jumps: {}";
    lText->Binding = "Hud.Jumps";
    lDoc.Root->AddChild(Move(lText));

    auto lImage = MakeUnique<UIImage>();
    lImage->FillBinding = "Hud.Speed";
    lDoc.Root->AddChild(Move(lImage));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 2u);
    CHECK(static_cast<const UIText*>(lBack.Root->GetChildren()[0].get())->Binding == OpaaxString("Hud.Jumps"));
    CHECK(static_cast<const UIImage*>(lBack.Root->GetChildren()[1].get())->FillBinding == OpaaxString("Hud.Speed"));

    // Written before U10: no key, no binding — the default (UI12).
    UICanvasFile::UICanvasDoc lOld;
    REQUIRE(UICanvasFile::Deserialize(OpaaxString(R"({"Version":1,"Root":{"Type":"UIPanel","Children":[{"Type":"UIText","Text":"Hi"}]}})"),
                                      lRegistry, lOld));
    CHECK(static_cast<const UIText*>(lOld.Root->GetChildren()[0].get())->Binding.IsEmpty());
}
