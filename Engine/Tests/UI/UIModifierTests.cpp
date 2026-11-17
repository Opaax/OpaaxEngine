// Suite: UI modifiers on widgets (UI/UIModifier.h). A widget owns its modifiers in the order they were
// added; each knows its owner while it is in the list, hears its attach and detach once each, and is
// given back on removal (what undo keeps). UIModifier::Create makes one of a registered class only.
#include <doctest.h>

#include "Object/ObjectClassRegistry.h"
#include "UI/UIModifier.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    Int32 s_Attaches = 0;
    Int32 s_Detaches = 0;

    /** Counts its hooks; carries one field. */
    class UITestModifier : public UIModifier
    {
        OPAAX_OBJECT(UITestModifier, UIModifier)

    public:
        float Amount = 1.f;

        OPAAX_PROPERTIES(UITestModifier, OPAAX_PROP(Amount))

        UITestModifier& SetAmount(const float InAmount) { Amount = InAmount; return *this; }

        /** Who owned me when I was told I left. */
        UIWidget* OwnerAtDetach = nullptr;

    protected:
        void OnAttach() override { ++s_Attaches; }
        void OnDetach() override { ++s_Detaches; OwnerAtDetach = GetOwner(); }
    };

    class UIOtherTestModifier final : public UITestModifier
    {
        OPAAX_OBJECT(UIOtherTestModifier, UITestModifier)
    };

    /** A widget class: a node's "Modifiers" must not make one. */
    class NotAModifier final : public UIWidget
    {
        OPAAX_OBJECT(NotAModifier, UIWidget)
    };

    void ResetCounts()
    {
        s_Attaches = 0;
        s_Detaches = 0;
    }
}

TEST_CASE("UIModifier: added in order, found by class and by name, its owner set")
{
    ResetCounts();
    TStrongObjectPtr<UIPanel> lPanel = NewObject<UIPanel>();

    UITestModifier& lFirst = lPanel->AddModifier<UITestModifier>().SetAmount(2.f);
    lFirst.Name = OPAAX_ID("First");
    UIOtherTestModifier& lSecond = lPanel->AddModifier<UIOtherTestModifier>();
    lSecond.Name = OPAAX_ID("Second");

    REQUIRE(lPanel->GetModifiers().size() == 2u);
    CHECK(lPanel->GetModifiers()[0].Get() == &lFirst);
    CHECK(lPanel->GetModifiers()[1].Get() == &lSecond);
    CHECK(lFirst.Amount == doctest::Approx(2.f));

    CHECK(lFirst.GetOwner() == lPanel.Get());
    CHECK(lSecond.GetOwner() == lPanel.Get());
    CHECK(s_Attaches == 2);

    // By class: the first of that class or a subclass.
    CHECK(lPanel->FindModifier<UITestModifier>() == &lFirst);
    CHECK(lPanel->FindModifier<UIOtherTestModifier>() == &lSecond);

    // By name: the class must match too.
    CHECK(lPanel->FindModifier<UITestModifier>(OPAAX_ID("Second")) == &lSecond);
    CHECK(lPanel->FindModifier<UIOtherTestModifier>(OPAAX_ID("First")) == nullptr);
    CHECK(lPanel->FindModifier<UITestModifier>(OPAAX_ID("Nope")) == nullptr);

    // Nothing on a widget without modifiers.
    CHECK(NewObject<UIImage>()->FindModifier<UITestModifier>() == nullptr);
}

TEST_CASE("UIModifier: removing gives it back detached, and it can be put back at its place")
{
    ResetCounts();
    TStrongObjectPtr<UIPanel> lPanel = NewObject<UIPanel>();

    UITestModifier& lA = lPanel->AddModifier<UITestModifier>();
    UITestModifier& lB = lPanel->AddModifier<UITestModifier>();
    UITestModifier& lC = lPanel->AddModifier<UITestModifier>();

    TStrongObjectPtr<UIModifier> lRemoved = lPanel->RemoveModifier(lB);
    REQUIRE(lRemoved.Get() == &lB);
    CHECK(lB.GetOwner() == nullptr);
    CHECK(lB.OwnerAtDetach == lPanel.Get());   // the hook still sees who it leaves
    CHECK(s_Detaches == 1);

    REQUIRE(lPanel->GetModifiers().size() == 2u);
    CHECK(lPanel->GetModifiers()[0].Get() == &lA);
    CHECK(lPanel->GetModifiers()[1].Get() == &lC);

    // Not mine any more: nothing to remove.
    CHECK_FALSE(lPanel->RemoveModifier(lB).IsValid());
    CHECK(s_Detaches == 1);

    // Undo puts it back where it was.
    CHECK(lPanel->AddModifier(Move(lRemoved), 1u) == &lB);
    CHECK(lPanel->GetModifiers()[1].Get() == &lB);
    CHECK(lB.GetOwner() == lPanel.Get());
    CHECK(s_Attaches == 4);
}

TEST_CASE("UIModifier: a modifier belongs to one widget at a time")
{
    TStrongObjectPtr<UIPanel> lOwner = NewObject<UIPanel>();
    TStrongObjectPtr<UIPanel> lOther = NewObject<UIPanel>();

    UITestModifier& lModifier = lOwner->AddModifier<UITestModifier>();

    // A second owner for the same object is refused; it stays where it is.
    CHECK(lOther->AddModifier(TStrongObjectPtr<UIModifier>(&lModifier)) == nullptr);
    CHECK(lOther->GetModifiers().empty());
    CHECK(lModifier.GetOwner() == lOwner.Get());

    CHECK(lOther->AddModifier(TStrongObjectPtr<UIModifier>()) == nullptr);
}

TEST_CASE("UIModifier: moving keeps it attached and changes only the order")
{
    ResetCounts();
    TStrongObjectPtr<UIPanel> lPanel = NewObject<UIPanel>();

    UITestModifier& lA = lPanel->AddModifier<UITestModifier>();
    UITestModifier& lB = lPanel->AddModifier<UITestModifier>();
    UITestModifier& lC = lPanel->AddModifier<UITestModifier>();

    REQUIRE(lPanel->MoveModifier(lC, 0u));
    CHECK(lPanel->GetModifiers()[0].Get() == &lC);
    CHECK(lPanel->GetModifiers()[1].Get() == &lA);
    CHECK(lPanel->GetModifiers()[2].Get() == &lB);

    // Past the end: last.
    REQUIRE(lPanel->MoveModifier(lC, 99u));
    CHECK(lPanel->GetModifiers()[2].Get() == &lC);

    CHECK(s_Attaches == 3);
    CHECK(s_Detaches == 0);
    CHECK(lC.GetOwner() == lPanel.Get());

    // Another widget's modifier is not moved here.
    TStrongObjectPtr<UIPanel> lOther = NewObject<UIPanel>();
    UITestModifier& lStranger = lOther->AddModifier<UITestModifier>();
    CHECK_FALSE(lPanel->MoveModifier(lStranger, 0u));
    CHECK(lPanel->GetModifiers().size() == 3u);
}

TEST_CASE("UIModifier: a destroyed widget detaches its modifiers once each")
{
    ResetCounts();

    TWeakObjectPtr<UIModifier> lWeak;
    {
        TStrongObjectPtr<UIPanel> lPanel = NewObject<UIPanel>();
        lPanel->AddModifier<UITestModifier>();
        lWeak = TWeakObjectPtr<UIModifier>(&lPanel->AddModifier<UITestModifier>());

        // A child's modifiers go with the child.
        UIWidget* lChild = lPanel->AddChild(NewObject<UIImage>());
        lChild->AddModifier<UITestModifier>();

        CHECK(lWeak.IsValid());
    }

    CHECK(s_Attaches == 3);
    CHECK(s_Detaches == 3);

    // Nobody owns it any more: a weak pointer reads null, never a dangling modifier.
    CHECK_FALSE(lWeak.IsValid());
}

TEST_CASE("UIModifier::Create makes a modifier of a registered class, and refuses the rest")
{
    ObjectClassRegistry lClasses;
    lClasses.Register<UIModifier>();
    lClasses.Register<UITestModifier>();
    lClasses.Register<NotAModifier>();

    CHECK(UIModifier::StaticClass().IsAbstract());

    const TStrongObjectPtr<UIModifier> lMade = UIModifier::Create(OPAAX_ID("UITestModifier"), lClasses);
    REQUIRE(lMade.IsValid());
    CHECK(lMade->IsA<UITestModifier>());
    CHECK(lMade->GetOwner() == nullptr);
    CHECK(lMade->bEnabled);

    CHECK_FALSE(UIModifier::Create(OPAAX_ID("UIOtherTestModifier"), lClasses).IsValid());   // not registered here
    CHECK_FALSE(UIModifier::Create(OPAAX_ID("Nope"), lClasses).IsValid());                  // no such class
    CHECK_FALSE(UIModifier::Create(OPAAX_ID("UIModifier"), lClasses).IsValid());            // abstract
    CHECK_FALSE(UIModifier::Create(OPAAX_ID("NotAModifier"), lClasses).IsValid());          // a widget, not a modifier
}

TEST_CASE("UIModifier: its saved fields are the base's, then its own")
{
    const auto lNames = UITestModifier::StaticClass().GetPropertyNames();
    REQUIRE(lNames.size() == 3u);
    CHECK(OpaaxString(lNames[0]) == "Name");
    CHECK(OpaaxString(lNames[1]) == "bEnabled");
    CHECK(OpaaxString(lNames[2]) == "Amount");
}
