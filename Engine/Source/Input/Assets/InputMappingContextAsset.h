#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Input/Assets/InputActionAsset.h"
#include "Input/InputKeyNames.h"   // EKeyCode saved by name
#include "Input/Mapping/InputTypes.h"
#include "Input/Mapping/InputTypesJson.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    // =============================================================================
    // InputMappingEntry — one key driving one action, through modifiers. The action is resolved
    //   once, when the context is added.
    // =============================================================================
    struct InputMappingEntry
    {
        /** Asset-relative ("Input/Jump.opaaxasset") or a mount. */
        TAssetRef<InputActionAsset> Action;

        /** Saved by name ("Space"). */
        EKeyCode Key = EKeyCode::None;

        /** Applied in order (DeadZone then Scalar differs from Scalar then DeadZone). */
        TDynArray<InputModifierData> Modifiers;

        /** The key is consumed: lower-priority contexts ignore it. */
        bool bConsume = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(InputMappingEntry, Action, Key, Modifiers, bConsume)

        OPAAX_PROPERTIES(InputMappingEntry,
                         OPAAX_PROP(Action).SetTooltip("The input action this key drives."),
                         OPAAX_PROP(Key).SetTooltip("Which physical key or mouse button."),
                         OPAAX_PROP(Modifiers).SetTooltip("Applied in order to this key's value only."),
                         OPAAX_PROP(bConsume).SetTooltip("Swallow this key from every lower-priority context."))
    };

    // =============================================================================
    // InputMappingContextAsset — a set of key mappings added as one unit, like Unreal's
    //   UInputMappingContext. A higher-priority context that consumes keys (a menu) blocks lower ones.
    //   InputMappingContext is the runtime form the evaluator keeps.
    // =============================================================================
    class InputMappingContextAsset final : public Asset
    {
        OPAAX_OBJECT(InputMappingContextAsset, Asset)

    public:
        /** Higher is evaluated (and consumes) first. */
        Int32 Priority = 0;

        TDynArray<InputMappingEntry> Mappings;

        OPAAX_PROPERTIES(InputMappingContextAsset,
                         OPAAX_PROP(Priority).SetTooltip("Higher contexts are evaluated first and consume keys first."),
                         OPAAX_PROP(Mappings).SetTooltip("Which key drives which action."))

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Mappings.size()); }
    };
}
