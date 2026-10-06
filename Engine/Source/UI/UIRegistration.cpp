// The UI module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "UI/UICanvasResource.h"
#include "UI/UISubsystem.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UISafeArea.h"
#include "UI/Widgets/UIStack.h"
#include "UI/Widgets/UIText.h"

namespace Opaax
{
    // Widget types a .opaaxui can use. Unknown types in a file are skipped.
    OPAAX_REGISTER_UI_WIDGET(UIPanel);
    OPAAX_REGISTER_UI_WIDGET(UIImage);
    OPAAX_REGISTER_UI_WIDGET(UIText);
    OPAAX_REGISTER_UI_WIDGET(UIButton);

    // Masks its children: white shows, black hides.
    OPAAX_REGISTER_UI_WIDGET(UIMask);

    // Keeps its children inside the screen's safe area.
    OPAAX_REGISTER_UI_WIDGET(UISafeArea);

    // Lays its children out along an axis.
    OPAAX_REGISTER_UI_WIDGET(UIStack);

    // An authored widget tree (.opaaxui). Each instance builds its own widgets from it.
    OPAAX_REGISTER_NAMED_RESOURCE(UICanvasResource, "UICanvas");

    // Created first: it routes raw input and consumes what the UI used, before input mapping runs.
    OPAAX_REGISTER_NAMED_GAME_INSTANCE_SUBSYSTEM(UISubsystem, "UI", 100);
}
