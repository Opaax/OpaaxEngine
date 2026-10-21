// Suite: UI widgets as Object classes — each widget type is a class under UIWidget, named as .opaaxui
// files name it; its properties are the base's then its own, once each (what files save and the UI
// panel draws); UIWidget::Create makes one of a registered class and refuses everything else.
#include <doctest.h>

#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "Object/ObjectClassRegistry.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UISafeArea.h"
#include "UI/Widgets/UIStack.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace
{
    /** A class that is an Object but not a widget: a file must not make one in a tree. */
    class NotAWidget final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(NotAWidget, GameInstanceSubsystem)
    };

    TDynArray<OpaaxString> PropertyNames(const ObjectClass& InClass)
    {
        TDynArray<OpaaxString> lNames;
        for (const char* lName : InClass.GetPropertyNames()) { lNames.emplace_back(lName); }
        return lNames;
    }
}

TEST_CASE("UIWidget classes: each widget type is a class under UIWidget, named as files name it")
{
    // The base is never made by itself: a file always names one of its classes.
    CHECK(UIWidget::StaticClass().IsAbstract());

    const ObjectClass* const lClasses[] = {
        &UIPanel::StaticClass(), &UIImage::StaticClass(), &UIText::StaticClass(), &UIButton::StaticClass(),
        &UIMask::StaticClass(), &UISafeArea::StaticClass(), &UIStack::StaticClass() };
    const char* const lNames[] = { "UIPanel", "UIImage", "UIText", "UIButton", "UIMask", "UISafeArea", "UIStack" };

    for (Uint64 lIndex = 0; lIndex < ArraySize(lClasses); ++lIndex)
    {
        CAPTURE(lNames[lIndex]);
        CHECK(lClasses[lIndex]->GetName() == OpaaxStringID(lNames[lIndex]));
        CHECK(lClasses[lIndex]->IsChildOf(UIWidget::StaticClass()));
        CHECK_FALSE(lClasses[lIndex]->IsAbstract());
    }
}

TEST_CASE("UIWidget classes: a leaf lists the base's properties first, then its own, once each")
{
    // Name, Rect, bVisible, bHitTestable, Opacity: every widget has them, in this order.
    const TDynArray<OpaaxString> lBase = PropertyNames(UIPanel::StaticClass());
    REQUIRE(lBase.size() == 5u);
    CHECK(lBase[0] == "Name");
    CHECK(lBase[4] == "Opacity");

    // A panel adds nothing; each leaf adds its own after the base's, and never repeats them.
    CHECK(PropertyNames(UIMask::StaticClass()).size()     == 5u + 2u);   // Texture, bShowMaskGraphic
    CHECK(PropertyNames(UISafeArea::StaticClass()).size() == 5u + 1u);   // Insets
    CHECK(PropertyNames(UIStack::StaticClass()).size()    == 5u + 5u);   // Axis, Spacing, Padding, ChildAlign, bFitContent
    CHECK(PropertyNames(UIImage::StaticClass()).size()    == 5u + 8u);   // Color ... FillBinding
    CHECK(PropertyNames(UIButton::StaticClass()).size()   == 5u + 8u);   // four colours, bEnabled, Texture, Sheet, Frame
    CHECK(PropertyNames(UIText::StaticClass()).size()     == 5u + 10u);

    const TDynArray<OpaaxString> lMask = PropertyNames(UIMask::StaticClass());
    CHECK(lMask[0] == "Name");
    CHECK(lMask[5] == "Texture");
    CHECK(lMask[6] == "bShowMaskGraphic");
}

TEST_CASE("UIWidget::Create makes a widget of a registered class, and refuses the rest")
{
    ObjectClassRegistry lClasses;
    lClasses.Register<UIWidget>();
    lClasses.Register<UIImage>();
    lClasses.Register<NotAWidget>();

    const TStrongObjectPtr<UIWidget> lImage = UIWidget::Create(OPAAX_ID("UIImage"), lClasses);
    REQUIRE(lImage.IsValid());
    CHECK(lImage->IsA<UIImage>());

    // Each call is a new widget, with the class's defaults.
    const TStrongObjectPtr<UIWidget> lOther = UIWidget::Create(OPAAX_ID("UIImage"), lClasses);
    REQUIRE(lOther.IsValid());
    CHECK(lOther.Get() != lImage.Get());

    CHECK_FALSE(UIWidget::Create(OPAAX_ID("UIText"), lClasses).IsValid());       // not registered here
    CHECK_FALSE(UIWidget::Create(OPAAX_ID("Nope"), lClasses).IsValid());         // no such class
    CHECK_FALSE(UIWidget::Create(OPAAX_ID("UIWidget"), lClasses).IsValid());     // abstract
    CHECK_FALSE(UIWidget::Create(OPAAX_ID("NotAWidget"), lClasses).IsValid());   // not a widget
}
