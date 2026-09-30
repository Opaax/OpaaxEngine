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
    // The undo steps of the component actions. Types are stored by authoring name, not entt type id
    // (the id is a hash of the C++ name, so a rename would break every step).
    // =============================================================================

    /**
     * A component was added. No payload: it was default-constructed, so redo has nothing to restore.
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
     * A component was removed. Its values are captured before removal, so undo restores them.
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
     * The values of one entity's components changed (the Inspector's field edits). Recorded by the
     * panel around the edit gesture, since property drawers write directly.
     * Values only: added/removed types and the entity name have their own steps.
     */
    struct EntityComponentsEdit
    {
        Guid                     EntityId;
        TDynArray<ComponentData> Before;
        TDynArray<ComponentData> After;

        /** Which document's world the entity is in (the Inspector and the prefab panel both record this type). */
        EUndoWorld               Scope = EUndoWorld::Active;

        /** Stores the entity's components (in its world) as the before state, dropping any open step. */
        void Begin(const EditorContext& InContext, Entity InEntity, EUndoWorld InScope);

        /**
         * Keeps only the components whose values differ, on both sides.
         * @return True if any did. A gesture that edited nothing returns false and is not a step
         */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Properties"; }
    };
}
