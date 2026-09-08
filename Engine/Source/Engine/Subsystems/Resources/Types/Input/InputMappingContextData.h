#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/Input/InputTypesJson.h"
#include "Engine/Subsystems/Input/InputKeyNames.h"   // EKeyCode serializes by LABEL
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    struct InputActionResource;   // only NAMED — TResourcePath never completes its parameter

    // =============================================================================
    // InputMappingEntry — one key driving one action, through a modifier pipeline.
    //
    //   THE ACTION IS A PATH, not a name, and that is what makes the editor field a resource
    //   picker rather than free text a typo can silently break. The name gameplay binds with
    //   comes from the action asset itself; this side only says WHICH asset.
    //
    //   Resolution happens ONCE, when the context is added — InputMappingSubsystem loads each
    //   action and hands the evaluator the resolved InputKeyBinding. Resolving a path per key
    //   per frame would be absurd, which is why the runtime and asset forms differ at all.
    // =============================================================================
    struct InputMappingEntry
    {
        /** Asset-relative ("Input/Jump.opaaxaction") or a mount. */
        TResourcePath<InputActionResource> Action;

        /** Written as a LABEL ("Space"), never an ordinal — a mapping file is hand-editable. */
        EKeyCode Key = EKeyCode::None;

        /** Applied IN ORDER. DeadZone-then-Scalar is not Scalar-then-DeadZone. */
        TDynArray<InputModifierData> Modifiers;

        /** Whether this key is swallowed from every LOWER-priority context. Per KEY. */
        bool bConsume = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(InputMappingEntry, Action, Key, Modifiers, bConsume)

        OPAAX_PROPERTIES(InputMappingEntry,
                         OPAAX_PROP(Action).SetTooltip("The .opaaxaction this key drives."),
                         OPAAX_PROP(Key).SetTooltip("Which physical key or mouse button."),
                         OPAAX_PROP(bConsume).SetTooltip("Swallow this key from every lower-priority context."))
    };

    // =============================================================================
    // InputMappingContextData — a set of key mappings pushed as one unit. The
    //   `.opaaxinputmap` payload.
    //
    //   PRIORITY LIVES HERE, not on the push call. Unreal puts it on AddMappingContext; ONE
    //   source is the rule this codebase keeps (L30), and an override parameter can arrive the
    //   day a caller wants one.
    //
    //   THIS IS THE UNIT OF REBINDING AND OF UI. A menu context at a higher priority whose
    //   entries consume is what stops Jump from firing while a menu is up — not a flag on the
    //   game, and not a second input path.
    // =============================================================================
    struct InputMappingContextData
    {
        /** Higher is evaluated — and consumes — first. */
        Int32 Priority = 0;

        TDynArray<InputMappingEntry> Mappings;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(InputMappingContextData, Priority, Mappings)

        /**
         * NO OPAAX_PROPERTIES for Mappings — it is a TDynArray and no property drawer draws a
         * list, the same split MoverData and SpriteSheetData both make. The panel owns the list;
         * the fold owns the selected entry.
         */
        OPAAX_PROPERTIES(InputMappingContextData,
                         OPAAX_PROP(Priority).SetTooltip("Higher contexts are evaluated first and consume keys first."))

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Mappings.size()); }
    };
}
