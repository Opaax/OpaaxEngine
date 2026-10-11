#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Input/Mapping/InputTypes.h"
#include "Input/Mapping/InputTypesJson.h"
#include "Input/InputKeyNames.h"   // EKeyCode saved by name
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct InputActionResource;

    // =============================================================================
    // InputMappingEntry — one key driving one action, through modifiers.
    //   The action is a path (resource picker in the editor), resolved once when the context is added.
    // =============================================================================
    struct InputMappingEntry
    {
        /** Asset-relative ("Input/Jump.opaaxaction") or a mount. */
        TResourcePath<InputActionResource> Action;

        /** Saved by name ("Space"). */
        EKeyCode Key = EKeyCode::None;

        /** Applied in order (DeadZone then Scalar differs from Scalar then DeadZone). */
        TDynArray<InputModifierData> Modifiers;

        /** The key is consumed: lower-priority contexts ignore it. */
        bool bConsume = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(InputMappingEntry, Action, Key, Modifiers, bConsume)

        OPAAX_PROPERTIES(InputMappingEntry,
                         OPAAX_PROP(Action).SetTooltip("The .opaaxaction this key drives."),
                         OPAAX_PROP(Key).SetTooltip("Which physical key or mouse button."),
                         OPAAX_PROP(bConsume).SetTooltip("Swallow this key from every lower-priority context."))
    };

    // =============================================================================
    // InputMappingContextData — a set of key mappings added as one unit (.opaaxinputmap).
    //   A higher-priority context that consumes keys (e.g. a menu) blocks lower ones.
    // =============================================================================
    struct InputMappingContextData
    {
        /** Higher is evaluated (and consumes) first. */
        Int32 Priority = 0;

        TDynArray<InputMappingEntry> Mappings;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(InputMappingContextData, Priority, Mappings)

        /**
         * Edited by the editor panel (lists are not drawn by the property system).
         */
        OPAAX_PROPERTIES(InputMappingContextData,
                         OPAAX_PROP(Priority).SetTooltip("Higher contexts are evaluated first and consume keys first."))

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Mappings.size()); }
    };
}
