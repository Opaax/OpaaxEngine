#pragma once

#include "Core/Maths/MathTypes.h"       // Vector2F / Matrix44F — the gizmo's delta
#include "Core/OpaaxTypes.h"            // Uint8
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"   // a component's authoring name — what a command carries
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
         * Place ONE instance of the prefab at InAbsPath into InOwnerMap, select all of it, and
         * record one undo step (⑦-C P1b).
         *
         * The map is a required argument for Create's reason (**WM2**), and the prefab is named by
         * an ABSOLUTE path because that is what a browser hands over; the marker written onto the
         * entities stores the ASSET-relative form (**MP8**), since an absolute one would bake this
         * machine's layout into every map that places the prefab.
         *
         * THE WHOLE INSTANCE IS SELECTED, not its first entity. That is what makes the gizmo move a
         * multi-entity prefab as one thing, and it costs nothing because the gizmo already
         * transforms a list (**UN2**) — ⑦-C **K10**'s stand-in for parenting, which this block
         * deliberately does not build.
         *
         * @return How many entities were created. 0 means refused, and the log says which reason.
         */
        /**
         * @param InAtWorld WHERE to put it, or null to keep the prefab's authored positions.
         *   Given, the whole instance is translated so its FIRST entity lands there — an anchor
         *   rather than a centroid, because an author dropping a turret means "the turret goes
         *   here", and the first entity is the one a prefab is built around. Applied BEFORE the
         *   undo step is captured, so a drop is one step and not a place-then-move pair.
         */
        Uint64 InstantiatePrefab(EditorContext& InContext, const OpaaxString& InAbsPath, MapId InOwnerMap,
                                 const Vector2F* InAtWorld = nullptr);

        /**
         * Write the SELECTION to InAbsPath as a new prefab, then REPLACE it with an instance of
         * that prefab (⑦-C P2).
         *
         * The replacement is what makes this "create a prefab" rather than "export one". Unity,
         * Unreal and Godot all do it, and the reason is the same in each: leaving the originals
         * unlinked produces entities that look like the prefab and silently ignore every later edit
         * to it — a half-built feature that only announces itself much later.
         *
         * WHICH MAP the instance lands in is the ORIGINALS' map, not the focused one: cutting a
         * prefab out of map A while map B happens to be focused must not move the result to B. A
         * selection of purely runtime-spawned entities has no map and falls back to the focused one.
         *
         * THE FILE IS WRITTEN FIRST and is not rolled back if the swap fails — and it is not part of
         * undo either (**K6**). A written prefab with the originals still in place is a recoverable
         * state; a swap with no file behind it is not.
         *
         * @param InAbsPath Where to write. Refused when it is outside the project's and the engine's
         *   asset trees, because no map could then reference it (**MP8**) — checked BEFORE writing.
         * @return true when the prefab was written AND the selection replaced.
         */
        bool CreatePrefabFromSelection(EditorContext& InContext, const OpaaxString& InAbsPath);

        /**
         * Put the selected instance entities back to their prefab's values (⑦-C P3).
         *
         * NOT a delete-and-replace: the entities keep their guids and go through
         * `MapFactory::Restore`, which is already "be this again" — it overwrites every component
         * the prefab names and REMOVES the registered ones it does not, so a component the author
         * added to an instance goes away and one they deleted comes back. Re-instantiating instead
         * would mint nothing new (the guids are derived) but would destroy and recreate entities
         * for an edit that changes only their contents.
         *
         * @param bInWholeInstance false reverts exactly the entities selected; true widens to every
         *   entity of the instances they belong to — Unity's "Revert All" on the instance. Two
         *   entries rather than a guess, since a multi-entity prefab makes them genuinely different
         *   and the selection cannot say which was meant.
         * @return How many entities were reverted. 0 means nothing selected carried a prefab link,
         *   or the prefab could not be resolved — the log says which.
         */
        Uint64 RevertToPrefab(EditorContext& InContext, bool bInWholeInstance);

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
         * Put one registered component on InEntity, by its AUTHORING name.
         *
         * By name rather than by `IComponentEntry*` because that is what a command can carry: an
         * interned id is plain data, a pointer into the registry is not (**the PanelIdParams rule**).
         *
         * These two were the last mutations reaching entt directly from a panel — the Inspector's
         * two popups did their own Add/Remove plus their own MarkChanged and log. Here, they route
         * like every other verb, which is what makes the choke point's claim true rather than
         * nearly true.
         *
         * @return false when the type is unknown, already present, or the edit was refused.
         */
        bool AddComponent(EditorContext& InContext, Entity InEntity, OpaaxStringID InTypeName);

        /**
         * Take one component off InEntity. An ESSENTIAL type is refused by the registry entry
         * itself (**I17**), not by this call remembering to check.
         *
         * @return false when the type is unknown, absent, essential, or the edit was refused.
         */
        bool RemoveComponent(EditorContext& InContext, Entity InEntity, OpaaxStringID InTypeName);

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
