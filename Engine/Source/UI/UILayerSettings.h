#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Tag/OpaaxTag.h"
#include "Core/Tag/OpaaxTagJson.h"

namespace Opaax
{
    /** The root of the UI layers' tags: every layer is a UI.Layer tag ("UI.Layer.Menu"). */
    inline OpaaxTag UILayerRoot() { static const OpaaxTag s_Tag("UI.Layer"); return s_Tag; }

    /** The layers the engine declares (Engine/Tags/EngineTags.cpp). Kept in statics: a tag interns its text. */
    inline OpaaxTag UILayerHud()     { static const OpaaxTag s_Tag("UI.Layer.Hud");     return s_Tag; }
    inline OpaaxTag UILayerMenu()    { static const OpaaxTag s_Tag("UI.Layer.Menu");    return s_Tag; }
    inline OpaaxTag UILayerDebug()   { static const OpaaxTag s_Tag("UI.Layer.Debug");   return s_Tag; }
    inline OpaaxTag UILayerLoading() { static const OpaaxTag s_Tag("UI.Layer.Loading"); return s_Tag; }

    // =============================================================================
    // UILayerSettings — one layer of the game's UI canvas, as the engine config lists it (UI.Layers).
    //   A higher Order draws on top. Its name is a UI.Layer tag; one more name is a sub-layer
    //   ("UI.Layer.Menu.Pause"): it is ordered among its parent's own trees, which are at 0, and never
    //   leaves its parent's place. A short name written before the root ("Menu") reads as UI.Layer.Menu.
    // =============================================================================
    struct UILayerSettings
    {
        /** What code mounts a tree in ("UI.Layer.Menu"); one more name for a sub-layer. */
        OpaaxTag Name;

        /** A higher order draws on top. A sub-layer's is among its parent's own trees, at 0. */
        Int32 Order = 0;

        friend void to_json(nlohmann::json& OutJson, const UILayerSettings& InLayer)
        {
            OutJson = nlohmann::json{ { "Name", InLayer.Name }, { "Order", InLayer.Order } };
        }

        friend void from_json(const nlohmann::json& InJson, UILayerSettings& OutLayer)
        {
            OutLayer = UILayerSettings{};
            if (const auto lName = InJson.find("Name"); lName != InJson.end())
            {
                OutLayer.Name = WithRoot(UILayerRoot(), lName->get<OpaaxTag>());
            }
            OutLayer.Order = InJson.value("Order", 0);
        }

        OPAAX_PROPERTIES(UILayerSettings,
                         OPAAX_PROP(Name).SetTooltip("What code mounts a tree in (\"UI.Layer.Menu\").\n"
                                                     "One more name for a sub-layer (\"UI.Layer.Menu.Pause\")."),
                         OPAAX_PROP(Order).SetTooltip("A higher order draws on top.\n"
                                                      "A sub-layer's is among its parent's own trees, at 0:\n"
                                                      "negative under them, positive over them."))
    };

    /** Hud 0, Menu 100, Debug 500, Loading 900. */
    inline TDynArray<UILayerSettings> DefaultUILayers()
    {
        return { UILayerSettings{ UILayerHud(), 0 }, UILayerSettings{ UILayerMenu(), 100 },
                 UILayerSettings{ UILayerDebug(), 500 }, UILayerSettings{ UILayerLoading(), 900 } };
    }
}
