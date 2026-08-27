#pragma once

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
         * @return The new entity, or an invalid one when refused (no world, PIE running, no map).
         */
        Entity Create(EditorContext& InContext, MapId InOwnerMap, const OpaaxString& InName);

        /** Destroy everything selected, and clear the selection. Refused while PIE runs. */
        void DestroySelected(EditorContext& InContext);

        /**
         * Frame the selection with the editor camera.
         *
         * Refused outside Edit, and that is not caution: a Play world is framed by its own
         * CameraComponent, so moving the editor camera there would do nothing visible at all.
         */
        void FocusSelected(EditorContext& InContext);
    }
}
