#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::ConfigLayer
{
    // =============================================================================
    // ConfigLayer — values put over a config's, shaped like its file: what a player changed in a
    //   settings menu over what the project ships. Objects go key by key; a list of items that have a
    //   "Name" (the audio buses) is reached by those names, so a layer that sets one item keeps the
    //   project's list: {"Audio": {"Buses": {"Music": {"Volume": 0.5}}}}. Pure JSON, no config.
    // =============================================================================

    /** One value of a layer, on its own. */
    struct Leaf
    {
        /** Where it goes, dotted: "Audio.Buses.Music.Volume". */
        OpaaxString Path;

        /** A layer holding only it: {"Audio": {"Buses": {"Music": {"Volume": 0.5}}}}. */
        nlohmann::json Layer;
    };

    /** Every value of InLayer, in order: an object is gone into (an empty one holds none), anything else is one. */
    TDynArray<Leaf> Leaves(const nlohmann::json& InLayer);

    /**
     * Puts InLayer over InOutValues: objects key by key, a list of named items by those names, anything
     * else replaced whole (a value of the wrong type is the reader's to refuse).
     * @return False when a value of InLayer has no place in InOutValues: a key it does not have, a name
     *   none of its items has. InOutValues may then be half changed.
     */
    bool Apply(nlohmann::json& InOutValues, const nlohmann::json& InLayer);

    /** Whether InValue is a list of items a layer reaches by name: objects, each with a string "Name". */
    bool IsNamedList(const nlohmann::json& InValue);
}
