// Suite: UI widget drawer dispatch (Editor/Extensions/DrawerRegistry.h): which drawer a widget
// resolves to, and that base and leaf property lists are drawn separately. The drawing itself lives
// in OpaaxEditorLib (not linked here); EditorService::RegisterNativeDrawers logs the count at runtime.
#include <doctest.h>

#include "Editor/Extensions/DrawerRegistry.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UISafeArea.h"
#include "UI/Widgets/UIStack.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    /** What the registry asks of a subject: "are you mine?". The ladder's job, done by the resolver. */
    template<typename TTarget>
    bool ResolvesTo(UIWidget& InWidget)
    {
        return TDrawerResolver<UIWidget, TTarget>::Resolve(InWidget) != nullptr;
    }
}

TEST_CASE("UIWidgetDrawers: every widget type resolves to ITS OWN drawer and no other")
{
    UIPanel     lPanel;
    UIImage     lImage;
    UIText      lText;
    UIButton    lButton;
    UIMask      lMask;
    UISafeArea  lSafe;
    UIStack     lStack;

    // The stack container, like the safe area.
    CHECK(ResolvesTo<UIStack>(lStack));
    CHECK_FALSE(ResolvesTo<UIStack>(lPanel));
    CHECK_FALSE(ResolvesTo<UIPanel>(lStack));

    // U5b's type, through the same gate: a container whose ONE field would be invisible under a
    // ladder for exactly the reason UIMask's was.
    CHECK(ResolvesTo<UISafeArea>(lSafe));
    CHECK_FALSE(ResolvesTo<UISafeArea>(lPanel));
    CHECK_FALSE(ResolvesTo<UIPanel>(lSafe));

    // The one their eyes found: a UIMask must resolve, or its Texture is never drawn.
    CHECK(ResolvesTo<UIMask>(lMask));
    CHECK_FALSE(ResolvesTo<UIMask>(lImage));
    CHECK_FALSE(ResolvesTo<UIMask>(lText));

    CHECK(ResolvesTo<UIText>(lText));
    CHECK_FALSE(ResolvesTo<UIText>(lImage));

    CHECK(ResolvesTo<UIImage>(lImage));
    CHECK_FALSE(ResolvesTo<UIImage>(lButton));

    CHECK(ResolvesTo<UIButton>(lButton));
    CHECK_FALSE(ResolvesTo<UIButton>(lPanel));

    CHECK(ResolvesTo<UIPanel>(lPanel));
    CHECK_FALSE(ResolvesTo<UIPanel>(lMask));
}

TEST_CASE("UIWidgetDrawers: a leaf's property list does NOT repeat the base's — two folds, not one")
{
    // The second half of the bug. The panel draws the base fold first and THEN asks the registry;
    // if it only did the latter (as the ladder did), none of these base fields would be editable.
    CHECK(PropertyCount<UIWidget>() == 5u);   // Name, Rect, bVisible, bHitTestable, Opacity

    CHECK(PropertyCount<UIMask>()     == 2u); // Texture + bShowMaskGraphic
    CHECK(PropertyCount<UISafeArea>() == 1u); // Insets, likewise
    CHECK(PropertyCount<UIStack>()    == 5u); // Axis, Spacing, Padding, ChildAlign, bFitContent
    CHECK(PropertyCount<UIText>()   > 1u);
    CHECK(PropertyCount<UIImage>()  == 8u); // Color, Texture, Sheet, Frame, Border, Fill, FillAmount, FillBinding
    CHECK(PropertyCount<UIButton>() == 8u); // four colours, bEnabled, Texture, Sheet, Frame

    // A UIPanel adds nothing of its own, so the base fold is the WHOLE of its inspector — which is
    // why "the registry drew nothing for it" must not read as "this widget has no fields".
    CHECK(PropertyCount<UIPanel>() == 0u);
}

TEST_CASE("UIWidgetDrawers: the resolver answers for the BASE type too, so nothing is unreachable")
{
    // UIWidget is registrable in its own right; a widget type added later with no drawer of its
    // own still resolves here, which is what keeps the panel from showing an empty pane.
    UIMask lMask;
    CHECK(ResolvesTo<UIWidget>(lMask));
}
