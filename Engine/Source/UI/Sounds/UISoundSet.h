#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Resources/Asset/Asset.h"

namespace Opaax
{
    /**
     * The sounds a widget plays, each a sound file ("Audio/Click.wav") or a clip definition
     * ("Audio/Clicks.opaaxasset"); empty plays nothing.
     */
    struct UISounds
    {
        OpaaxString Hover;   // the pointer comes over it (not again when a press is released)
        OpaaxString Press;   // pressed
        OpaaxString Click;   // its action fires (a button's click)
        OpaaxString Show;    // shown
        OpaaxString Hide;    // hidden with Hide()
        float       Volume = 1.f;

        // _WITH_DEFAULT: a missing key keeps its default.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UISounds, Hover, Press, Click, Show, Hide, Volume)

        OPAAX_PROPERTIES(UISounds,
                         OPAAX_PROP(Hover).SetTooltip("Played when the pointer comes over it (a sound file or a clip definition)."),
                         OPAAX_PROP(Press).SetTooltip("Played when it is pressed."),
                         OPAAX_PROP(Click).SetTooltip("Played when its action fires: a button's click."),
                         OPAAX_PROP(Show).SetTooltip("Played when it is shown."),
                         OPAAX_PROP(Hide).SetTooltip("Played when it is hidden with Hide()."),
                         OPAAX_PROP(Volume).SetRange(0.f, 4.f).SetDragStep(0.01f)
                                           .SetTooltip("Multiplies every sound's volume (and a definition's own)."))
    };

    // =============================================================================
    // UISoundSet — a widget's sounds as an asset (a "UI Sounds" preset): many buttons name one and share
    //   it, as a UI Transition shares a feel. Sounds stay out of the button style: any widget can play them.
    // =============================================================================
    class UISoundSet final : public Asset
    {
        OPAAX_OBJECT(UISoundSet, Asset)

    public:
        UISounds Sounds;

        OPAAX_PROPERTIES(UISoundSet,
                         OPAAX_PROP(Sounds).SetTooltip("What plays on hover, press, click, show and hide."))

        //~Begin Object interface
        /** Keeps Volume at 0 or more. */
        void PostEditChange() override;
        //~End Object interface
    };
}
