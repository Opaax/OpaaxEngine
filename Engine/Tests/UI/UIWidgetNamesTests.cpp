// Suite: UIWidgetNames — names within one widget tree, so UIWidget::FindByName finds what was meant.
#include <doctest.h>

#include "UI/UIWidget.h"
#include "UI/UIWidgetNames.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    TStrongObjectPtr<UIWidget> Named(const char* InName)
    {
        TStrongObjectPtr<UIWidget> lWidget = NewObject<UIPanel>();
        lWidget->Name = OpaaxString(InName);
        return lWidget;
    }
}

TEST_CASE("UIWidgetNames: a copied subtree is named anew, children included")
{
    TStrongObjectPtr<UIWidget> lRoot = Named("Root");
    UIWidget* lMenu = lRoot->AddChild(Named("Menu"));
    lMenu->AddChild(Named("Resume"));

    // A copy of Menu, as Duplicate or Paste makes it.
    TStrongObjectPtr<UIWidget> lCopy = Named("Menu");
    lCopy->AddChild(Named("Resume"));
    lCopy->AddChild(Named(""));

    UIWidgetNames::NameUniquely(*lRoot, *lCopy);

    CHECK(lCopy->Name == OpaaxString("Menu 1"));
    CHECK(lCopy->GetChildren()[0]->Name == OpaaxString("Resume 1"));
    CHECK(lCopy->GetChildren()[1]->Name.IsEmpty());

    // Pasted under the first branch, the original is still the one found.
    UIWidget* lPasted = lRoot->AddChild(Move(lCopy), 0);
    CHECK(lRoot->FindByName(OpaaxString("Resume")) == lMenu->GetChildren()[0].Get());
    CHECK(lRoot->FindByName(OpaaxString("Resume 1")) == lPasted->GetChildren()[0].Get());
    CHECK(UIWidgetNames::FindDuplicates(*lRoot).empty());
}

TEST_CASE("UIWidgetNames: two new widgets of one name do not share it")
{
    TStrongObjectPtr<UIWidget> lRoot = Named("Root");

    TStrongObjectPtr<UIWidget> lCopy = Named("Row");
    lCopy->AddChild(Named("Label"));
    lCopy->AddChild(Named("Label"));

    UIWidgetNames::NameUniquely(*lRoot, *lCopy);

    CHECK(lCopy->GetChildren()[0]->Name == OpaaxString("Label"));
    CHECK(lCopy->GetChildren()[1]->Name == OpaaxString("Label 1"));
}

TEST_CASE("UIWidgetNames: IsTaken can leave out the widget being renamed")
{
    TStrongObjectPtr<UIWidget> lRoot = Named("Root");
    UIWidget* lButton = lRoot->AddChild(Named("Button"));

    CHECK(UIWidgetNames::IsTaken(*lRoot, OpaaxString("Button")));
    CHECK_FALSE(UIWidgetNames::IsTaken(*lRoot, OpaaxString("Button"), lButton));
}

TEST_CASE("UIWidgetNames: FindDuplicates names each shared name once")
{
    TStrongObjectPtr<UIWidget> lRoot = Named("Root");
    lRoot->AddChild(Named("Title"));
    UIWidget* lPanel = lRoot->AddChild(Named("Panel"));
    lPanel->AddChild(Named("Title"));
    lPanel->AddChild(Named("Title"));
    lPanel->AddChild(Named(""));
    lPanel->AddChild(Named(""));

    const TDynArray<OpaaxString> lDuplicates = UIWidgetNames::FindDuplicates(*lRoot);
    REQUIRE(lDuplicates.size() == 1);
    CHECK(lDuplicates.front() == OpaaxString("Title"));
}
