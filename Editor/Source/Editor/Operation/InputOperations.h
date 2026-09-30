#pragma once

#include "Core/OpaaxTypes.h"
#include "Engine/Subsystems/Input/InputCodes.h"   // EKeyCode
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextData.h"

namespace Opaax
{
    struct InputActionData;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MakeComposite2D — the four mappings of a WASD-style composite. A pure, header-only function
    //   so tests can check it (a swapped pair walks the character sideways).
    //   The raw value is in x: +X is plain, -X negates, +Y swizzles, -Y does both.
    //
    //   @param InTemplate The row whose action and consume flag the four copy.
    //   @return Right, Left, Up, Down, in that order.
    // =============================================================================
    inline TDynArray<InputMappingEntry> MakeComposite2D(const InputMappingEntry& InTemplate,
                                                        const EKeyCode InUp,   const EKeyCode InDown,
                                                        const EKeyCode InLeft, const EKeyCode InRight)
    {
        auto lMake = [&InTemplate](const EKeyCode InKey, const bool bInNegate, const bool bInSwizzle)
        {
            InputMappingEntry lEntry;
            lEntry.Action   = InTemplate.Action;
            lEntry.Key      = InKey;
            lEntry.bConsume = InTemplate.bConsume;

            if (bInNegate)
            {
                InputModifierData lNegate;
                lNegate.Type = EInputModifier::Negate;
                lEntry.Modifiers.emplace_back(lNegate);
            }

            if (bInSwizzle)
            {
                InputModifierData lSwizzle;
                lSwizzle.Type = EInputModifier::Swizzle;
                lEntry.Modifiers.emplace_back(lSwizzle);
            }

            return lEntry;
        };

        return { lMake(InRight, false, false),
                 lMake(InLeft,  true,  false),
                 lMake(InUp,    false, true),
                 lMake(InDown,  true,  true) };
    }

    // =============================================================================
    // InputActionOps — the input action editor's actions. Each changes the open document and
    //   records its own undo step.
    // =============================================================================
    namespace InputActionOps
    {
        /**
         * Closes an in-place edit of the open action (the panel's DrawProperties).
         * @param InBefore The data when the edit started
         * @return True if a step was recorded, false if nothing changed
         */
        bool CommitEdit(EditorContext& InContext, const InputActionData& InBefore);

        /** Adds a Scalar modifier. @return False when nothing is open */
        bool AddModifier(EditorContext& InContext);

        /** Removes the modifier at InIndex. @return False when nothing is open or the index is out of range */
        bool RemoveModifier(EditorContext& InContext, Uint32 InIndex);

        /**
         * Moves the modifier at InIndex by InDelta places, clamped to the list. Order matters
         * (DeadZone then Scalar differs from Scalar then DeadZone).
         * @return False when it would not move
         */
        bool MoveModifier(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Writes the open action to its file, updates the dirty baseline, and reloads the resource.
         * @return False when nothing is open or the write failed
         */
        bool Save(EditorContext& InContext);
    }

    // =============================================================================
    // InputMapOps — the mapping context editor's actions (rebinding). Actions are not touched:
    //   a key changes, the action it triggers does not.
    // =============================================================================
    namespace InputMapOps
    {
        /** Adds an empty mapping, ready for an action drop. */
        bool AddMapping(EditorContext& InContext);

        /** Removes the mapping at InIndex. */
        bool RemoveMapping(EditorContext& InContext, Uint32 InIndex);

        /** Moves the mapping at InIndex by InDelta places, clamped to the list. */
        bool MoveMapping(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Closes an in-place edit of the mapping at InIndex (the panel's DrawProperties).
         * @param InBefore The entry when the edit started
         * @return True if a step was recorded, false if nothing changed
         */
        bool CommitMappingEdit(EditorContext& InContext, Uint32 InIndex, const InputMappingEntry& InBefore);

        /**
         * Replaces the mapping at InTemplateIndex with the four mappings of a WASD-style 2D composite:
         * Right (no modifier), Left (Negate), Up (Swizzle), Down (Negate + Swizzle).
         * @param InTemplateIndex The mapping whose action and consume flag the four copy
         * @return False when nothing is open, the index is out of range, or it has no action
         */
        bool AddComposite2D(EditorContext& InContext, Uint32 InTemplateIndex,
                            EKeyCode InUp, EKeyCode InDown, EKeyCode InLeft, EKeyCode InRight);

        /** Adds a Scalar modifier to the mapping at InIndex. */
        bool AddMappingModifier(EditorContext& InContext, Uint32 InIndex);

        /** Removes modifier InModifierIndex from the mapping at InIndex. */
        bool RemoveMappingModifier(EditorContext& InContext, Uint32 InIndex, Uint32 InModifierIndex);

        /** The priority the whole context is pushed at. @return False when nothing changed */
        bool SetPriority(EditorContext& InContext, Int32 InPriority);

        /**
         * Writes the open context to its file, updates the dirty baseline, and reloads the resource.
         * @return False when nothing is open or the write failed
         */
        bool Save(EditorContext& InContext);
    }
}
