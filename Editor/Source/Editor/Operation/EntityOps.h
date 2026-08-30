#pragma once

#include "Core/Maths/MathTypes.h"       // Vector2F / Matrix44F — the gizmo's delta
#include "Core/OpaaxTypes.h"            // Uint8
#include "Core/String/OpaaxString.hpp"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    class Entity;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // EntityOps — the verbs that CREATE, DESTROY or FRAME entities, beside MapOps and for the same
    //   stated reason: a verb duplicated per call site is a verb that drifts. The Hierarchy's
    //   context menus, the Edit menu and the viewport's keys all reach these, never the World.
    //
    //   THIS IS THE ONE NAMED MUTATION CHOKE POINT the sequence's ⑤ asks ② and ③ to hold. Undo is
    //   then "wrap these", not a twenty-call-site hunt — and `Editor.md` §7's promise that panels
    //   route edits through identifiable mutation points becomes true rather than aspirational.
    //   The Inspector is the one edit that cannot come through here (a drawer writes straight
    //   through a TComponent&), which is exactly why World::GetRevision exists.
    //
    //   EVERY MUTATOR GATES ON MapOps::CanEdit. Authoring into a Play clone would be written over
    //   by the next Stop, so it is refused with a Warn naming the verb rather than half-working.
    // =============================================================================
    namespace EntityOps
    {
        /**
         * Create an entity owned by InOwnerMap, select it, and mark the world changed.
         *
         * THE MAP IS A REQUIRED ARGUMENT, which is how **WM2** is closed by construction: an entity
         * created without one lands in the Hierarchy's `(runtime - not saved)` bucket and no Save
         * can ever write it. That was a live bug in `SandboxPanel`, and the fix is that the caller
         * has to say which map it is authoring into — the Hierarchy's header knows because it was
         * clicked, the menu command uses the focused map.
         *
         * It carries no components: the Inspector's Add Component is how one gains any, and the
         * viewport draws it as an icon meanwhile so it is never invisible.
         *
         * InName is made UNIQUE in the world ("Entity", then "Entity 1", ...). Names are a debug
         * label and may legally repeat, but a multi-selection reading "3 selected - editing Entity"
         * cannot say WHICH, so the default has to be distinguishable on sight.
         *
         * @return The new entity, or an invalid one when refused (no world, PIE running, no map).
         */
        Entity Create(EditorContext& InContext, MapId InOwnerMap, const OpaaxString& InName);

        /**
         * Rename one entity. Empty input is refused — a nameless row in the Hierarchy is
         * unclickable in practice and tells an author nothing.
         *
         * The name is NOT uniquified here, unlike Create's: this is an explicit choice by the
         * author, and silently altering what they typed is worse than two rows agreeing.
         */
        void Rename(EditorContext& InContext, Entity InEntity, const OpaaxString& InName);

        /** Destroy everything selected, and clear the selection. Refused while PIE runs. */
        void DestroySelected(EditorContext& InContext);

        /**
         * WHERE a delta's rotation and scale act on a multi-selection.
         *
         * Shared — the delta is a world-space map, so entities ORBIT whatever point it was built
         * about and the selection keeps its formation. Individual — each entity turns about itself
         * and none of them move. Only the second needs saying, because the first falls out of
         * applying one matrix to everything.
         */
        enum class ETransformOrigin : Uint8
        {
            Shared,
            Individual
        };

        /**
         * ONE DRAG FRAME, described completely: what moved, in which frame, and about what.
         *
         * A struct rather than three arguments because these are not independent knobs — they are
         * one answer to "what did the gizmo just do", and ⑤ records exactly this to replay a drag.
         */
        struct TransformDelta
        {
            /** World-space map from each entity's old placement to its new one. */
            Matrix44F Matrix = Matrix44F(1.f);

            /**
             * The frame Matrix's LINEAR part is expressed in — the pose the gizmo was seated with,
             * radians. A scale arrives as `R·S·R⁻¹`; without R there is no way to recover S, and
             * guessing the entity's own rotation instead is what made a multi-selection drift.
             */
            float FrameRad = 0.f;

            ETransformOrigin Origin = ETransformOrigin::Shared;
        };

        /**
         * Apply a world-space transform delta to everything selected — the gizmo's drag (③).
         *
         * ONE VERB FOR ALL THREE MODES, and a matrix rather than three scalars, because that is what
         * the gizmo actually produces: InDelta maps each entity's old placement to its new one, so
         * translation, rotation ABOUT THE PIVOT and scaling about the pivot all arrive as the same
         * value. A multi-selection therefore keeps its layout with no special case, and a single
         * entity — whose pivot is its own origin — falls out of the identical code path.
         *
         * INCREMENTAL rather than absolute: it is the form ⑤ coalesces, since a drag is many of
         * these and deltas compose by multiplication.
         *
         * Rotation and scale are read off the delta's own basis vectors (angle and length), which is
         * why this stays free of ImGuizmo — the choke point speaks the engine's vocabulary, not a
         * vendor's.
         *
         * Everything the mutation needs travels in InDelta, so the call is REPLAYABLE — ⑤ must be
         * able to re-apply a recorded drag without the toolbar's current state changing what it
         * means.
         */
        void TransformSelected(EditorContext& InContext, const TransformDelta& InDelta);

        /**
         * Frame the selection with the editor camera.
         *
         * Refused outside Edit, and that is not caution: a Play world is framed by its own
         * CameraComponent, so moving the editor camera there would do nothing visible at all.
         */
        void FocusSelected(EditorContext& InContext);
    }
}
