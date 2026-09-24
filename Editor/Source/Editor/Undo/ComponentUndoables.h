// ComponentUndoables.h
#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/Undo/UndoWorld.h"
#include "World/Entity/Entity.h"
#include "World/Serialization/MapData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The steps the two COMPONENT verbs record. By authoring NAME, never by entt type id: the id
    // is a hash of a C++ type name, so renaming the type would orphan every step — the rule
    // ComponentData already follows for the same reason.
    // =============================================================================

    /**
     * A component was put on an entity.
     *
     * NO PAYLOAD: `EntityOps::AddComponent` default-constructs it, so redo has nothing to restore.
     * Whatever was typed into it afterwards is a later step, and undone in its own turn.
     */
    struct ComponentAdd
    {
        Guid          EntityId;
        OpaaxStringID TypeName;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Add Component"; }
    };

    /**
     * A component was taken off an entity.
     *
     * ITS VALUES RIDE ALONG, captured before the removal — without them undo would bring the type
     * back at its defaults, which reads as data loss rather than as an undo.
     */
    struct ComponentRemove
    {
        Guid           EntityId;
        OpaaxStringID  TypeName;
        nlohmann::json Data;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Remove Component"; }
    };

    /**
     * The VALUES of one entity's components changed — the Inspector's field edits.
     *
     * THE ONE MUTATION WITH NO VERB. A TPropertyDrawer writes straight through a `T&` (**I15**), so
     * the panel that hosts the drawers is what records it, bracketing the edit gesture it already
     * tracks for MarkChanged. It needs to know only WHICH ENTITY, never which field — and the
     * Inspector draws exactly one.
     *
     * VALUES ONLY, which is what keeps it out of the other verbs' way. A type on one side and not
     * the other was added or removed, and those are ComponentAdd's and ComponentRemove's steps; the
     * entity's NAME is EntityRename's. So committing a name in the same frame the gesture closes
     * records nothing here, instead of a second step that undoes the same rename.
     */
    struct EntityComponentsEdit
    {
        Guid                     EntityId;
        TDynArray<ComponentData> Before;
        TDynArray<ComponentData> After;

        /** Which document's world the entity lives in (P8 V4) — the Inspector's and the prefab panel's record the same type. */
        EUndoWorld               Scope = EUndoWorld::Active;

        /** Cache the entity's components (in ITS world) as the BEFORE half, dropping any step left open. */
        void Begin(const EditorContext& InContext, Entity InEntity, EUndoWorld InScope);

        /**
         * Keep only the components whose PAYLOAD differs, on both sides.
         *
         * @return true when any did. A gesture that edited nothing — a button, a popup, a click
         *   that missed, a drag that returned home — answers false and is not a step.
         */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Properties"; }
    };
}
