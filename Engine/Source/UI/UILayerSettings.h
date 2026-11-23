#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Tag/OpaaxTag.h"
#include "Core/Tag/OpaaxTagJson.h"

namespace Opaax
{
    // =============================================================================
    // UILayerSettings — one layer of the game's UI canvas, as the engine config lists it (UI.Layers).
    //   A higher Order draws on top. A dotted name is a sub-layer ("Menu.Pause"): it is ordered among
    //   its parent's own trees, which are at 0, and never leaves its parent's place.
    // =============================================================================
    struct UILayerSettings
    {
        /** What code mounts a tree in ("Menu"); dotted for a sub-layer ("Menu.Pause"). */
        OpaaxTag Name;

        /** A higher order draws on top. A sub-layer's is among its parent's own trees, at 0. */
        Int32 Order = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UILayerSettings, Name, Order)

        OPAAX_PROPERTIES(UILayerSettings,
                         OPAAX_PROP(Name).SetTooltip("What code mounts a tree in (\"Menu\").\n"
                                                     "Dotted for a sub-layer (\"Menu.Pause\")."),
                         OPAAX_PROP(Order).SetTooltip("A higher order draws on top.\n"
                                                      "A sub-layer's is among its parent's own trees, at 0:\n"
                                                      "negative under them, positive over them."))
    };

    /** The layers a new config lists. Kept in statics: a tag interns its text. */
    inline OpaaxTag UILayerHud()     { static const OpaaxTag s_Tag("Hud");     return s_Tag; }
    inline OpaaxTag UILayerMenu()    { static const OpaaxTag s_Tag("Menu");    return s_Tag; }
    inline OpaaxTag UILayerDebug()   { static const OpaaxTag s_Tag("Debug");   return s_Tag; }
    inline OpaaxTag UILayerLoading() { static const OpaaxTag s_Tag("Loading"); return s_Tag; }

    /** Hud 0, Menu 100, Debug 500, Loading 900. */
    inline TDynArray<UILayerSettings> DefaultUILayers()
    {
        return { UILayerSettings{ UILayerHud(), 0 }, UILayerSettings{ UILayerMenu(), 100 },
                 UILayerSettings{ UILayerDebug(), 500 }, UILayerSettings{ UILayerLoading(), 900 } };
    }
}
