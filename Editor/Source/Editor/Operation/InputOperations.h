#pragma once

#include "Core/OpaaxTypes.h"
#include "Engine/Subsystems/Input/InputCodes.h"   // EKeyCode — AddComposite2D names four
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextData.h"

namespace Opaax
{
    struct InputActionData;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MakeComposite2D — the four mappings of a WASD-style composite, as a PURE function.
    //
    //   HOISTED OUT of the verb below so it can be gated. Everything else in these panels is
    //   plumbing a test cannot reach (EditorContext is ~29 references, and the editor needs a GL
    //   context), but WHICH DIRECTION GETS WHICH MODIFIER is real logic that fails silently: a
    //   swapped pair walks the character sideways when you press up, which is the same class of
    //   defect as the diagonal that survived a passing test in B1.
    //
    //   Header-only and ImGui-free on purpose — that is what lets OpaaxTests reach it at all,
    //   the way it already reaches EditorGizmo and EditorRectGeometry.
    //
    //   The raw value rides in x (**IM5**), so: +X is bare, -X negates, +Y swizzles, -Y does both.
    //
    //   @param InTemplate The row whose ACTION and consume flag the four inherit.
    //   @return Right, Left, Up, Down — in that order.
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
    // InputActionOps — the action editor's VERBS. Beside MoveModeOps, whose shape they take.
    //
    //   Each one mutates the open document and records its own undo step (**UN1**).
    // =============================================================================
    namespace InputActionOps
    {
        /**
         * Close an in-place edit of the open action — the panel's DrawProperties gesture.
         *
         * @param InBefore The data as it was when the gesture opened.
         * @return true when a step was recorded; false when the fold changed nothing.
         */
        bool CommitEdit(EditorContext& InContext, const InputActionData& InBefore);

        /** Append a Scalar modifier. @return false when nothing is open. */
        bool AddModifier(EditorContext& InContext);

        /** Remove the modifier at InIndex. @return false when nothing is open or it is past the end. */
        bool RemoveModifier(EditorContext& InContext, Uint32 InIndex);

        /**
         * Move the modifier at InIndex by InDelta places, clamped to the list.
         *
         * Order is NOT cosmetic: DeadZone-then-Scalar is not Scalar-then-DeadZone, because a dead
         * zone's thresholds are in the un-scaled range (**IM5**).
         *
         * @return false when the move would leave it where it is.
         */
        bool MoveModifier(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Write the open action back to its file, rebase the dirty marker and publish it.
         *
         * @return false when nothing is open or the write failed.
         */
        bool Save(EditorContext& InContext);
    }

    // =============================================================================
    // InputMapOps — the mapping context editor's verbs. MoverOps' shape.
    //
    //   THESE ARE WHAT REBINDING IS. Nothing here touches an action: a key changes, the action it
    //   reaches does not, which is why the two are separate assets (**IM9**).
    // =============================================================================
    namespace InputMapOps
    {
        /** Append an empty mapping, ready to have an action dropped on it. */
        bool AddMapping(EditorContext& InContext);

        /** Remove the mapping at InIndex. */
        bool RemoveMapping(EditorContext& InContext, Uint32 InIndex);

        /** Move the mapping at InIndex by InDelta places, clamped to the list. */
        bool MoveMapping(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Close an in-place edit of the mapping at InIndex — the panel's DrawProperties gesture.
         *
         * @param InBefore The entry as it was when the gesture opened.
         * @return true when a step was recorded; false when nothing changed.
         */
        bool CommitMappingEdit(EditorContext& InContext, Uint32 InIndex, const InputMappingEntry& InBefore);

        /**
         * Append the FOUR mappings of a WASD-style 2D composite for the action at InIndex's entry.
         *
         * The authoring cost of IM5's design, paid once here instead of by every author: a
         * composite is four bindings plus Negate/Swizzle, and knowing which pair needs which is
         * exactly the knowledge a person should not have to carry. One click, no new file format.
         *
         * REPLACES the mapping at InTemplateIndex with four, all pointing at the action it named:
         * Right (no modifiers), Left (Negate), Up (Swizzle), Down (Negate + Swizzle). One entry in,
         * four out — so the author drops an action on one row and presses one button.
         *
         * @param InTemplateIndex The mapping whose ACTION and consume flag the four inherit.
         * @return false when nothing is open, the index is past the end, or it names no action.
         */
        bool AddComposite2D(EditorContext& InContext, Uint32 InTemplateIndex,
                            EKeyCode InUp, EKeyCode InDown, EKeyCode InLeft, EKeyCode InRight);

        /** Append a Scalar modifier to the mapping at InIndex. */
        bool AddMappingModifier(EditorContext& InContext, Uint32 InIndex);

        /** Remove modifier InModifierIndex from the mapping at InIndex. */
        bool RemoveMappingModifier(EditorContext& InContext, Uint32 InIndex, Uint32 InModifierIndex);

        /** The priority the whole context is pushed at. @return false when nothing changed. */
        bool SetPriority(EditorContext& InContext, Int32 InPriority);

        /**
         * Write the open context back to its file, rebase the dirty marker and publish it.
         *
         * @return false when nothing is open or the write failed.
         */
        bool Save(EditorContext& InContext);
    }
}
