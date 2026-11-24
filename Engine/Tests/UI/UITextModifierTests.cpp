// Suite: the text stage of UI modifiers (UIModifier::ModifyText). A text widget builds what it shows
// from its Text, its Binding's value, then its enabled text modifiers in list order; the authored Text
// never changes.
#include <doctest.h>

#include "UI/UIBinding.h"
#include "UI/UICanvas.h"
#include "UI/UIModifier.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace
{
    /** Adds Suffix at the end of the text, or fills its first {} when bFill. */
    class UISuffixTestModifier final : public UIModifier
    {
        OPAAX_OBJECT(UISuffixTestModifier, UIModifier)

    public:
        OpaaxString Suffix;
        bool        bFill = false;

        OPAAX_PROPERTIES(UISuffixTestModifier, OPAAX_PROP(Suffix), OPAAX_PROP(bFill))

        UISuffixTestModifier& Set(const char* InSuffix, const bool bInFill = false)
        {
            Suffix = InSuffix;
            bFill  = bInFill;
            InvalidateOwnerContent();
            return *this;
        }

    protected:
        void ModifyText(OpaaxString& InOutText) const override
        {
            InOutText = bFill ? FormatBoundText(InOutText, Suffix) : InOutText + Suffix;
        }
    };

    struct Scores
    {
        Uint32 Lives = 3;

        OPAAX_PROPERTIES(Scores, OPAAX_PROP(Lives))
    };

    struct Fixture
    {
        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Canvas.Update();
        }

        UIText& AddText(const char* InText)
        {
            auto lOwned = NewObject<UIText>();
            lOwned->Text = InText;
            return static_cast<UIText&>(*Canvas.Root().AddChild(Move(lOwned)));
        }

        UICanvas Canvas{ 1080.f };
    };
}

TEST_CASE("UIModifier::ModifyText: text modifiers run in list order, the disabled ones skipped")
{
    Fixture lF;
    UIText& lText = lF.AddText("x");

    UISuffixTestModifier& lA = lText.AddModifier<UISuffixTestModifier>().Set("A");
    UISuffixTestModifier& lB = lText.AddModifier<UISuffixTestModifier>().Set("B");
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "xAB");
    CHECK(lText.Text == "x");   // what a save writes

    lText.MoveModifier(lB, 0);
    lText.InvalidateContent();
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "xBA");

    lA.bEnabled = false;
    lA.PostEditChange();
    lText.InvalidateContent();
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "xB");

    // A changed modifier is seen only once it invalidates: no rebuild, no new text.
    lB.Suffix = "C";
    UICanvasStats lStats = lF.Canvas.Update();
    CHECK(lStats.Rebuilds == 0);
    CHECK(lText.GetDisplayText() == "xB");

    lB.Set("C");
    lStats = lF.Canvas.Update();
    CHECK(lStats.Rebuilds == 1);
    CHECK(lText.GetDisplayText() == "xC");
}

TEST_CASE("UIModifier::ModifyText: the binding's value goes in first, then the modifiers")
{
    Fixture lF;
    Scores  lScores;
    lF.Canvas.Bindings().Add(OPAAX_ID("Game"), MakeBindingReader(lScores));

    UIText& lText = lF.AddText("Lives: {} on {}");
    lText.Binding = "Game.Lives";
    lText.AddModifier<UISuffixTestModifier>().Set("Tuesday", true);
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "Lives: 3 on Tuesday");
    CHECK(lText.Text == "Lives: {} on {}");

    // The value changes: the modifiers run again on the new text.
    lScores.Lives = 2;
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "Lives: 2 on Tuesday");

    // The value holds: nothing is rebuilt.
    const UICanvasStats lStats = lF.Canvas.Update();
    CHECK(lStats.Rebuilds == 0);
}

TEST_CASE("UIModifier::ModifyText: with no {} left, a filling modifier replaces the text")
{
    Fixture lF;
    UIText& lText = lF.AddText("Score");
    lText.AddModifier<UISuffixTestModifier>().Set("24 November 2026", true);
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "24 November 2026");
}

TEST_CASE("UIModifier::ModifyText: removed, the text is shown as written again")
{
    Fixture lF;
    UIText& lText = lF.AddText("x");
    UISuffixTestModifier& lA = lText.AddModifier<UISuffixTestModifier>().Set("A");
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "xA");

    TStrongObjectPtr<UIModifier> lBack = lText.RemoveModifier(lA);
    lText.InvalidateContent();
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "x");
}
