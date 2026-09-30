// EntityUndoables.h
#pragma once

#include "Core/GUID/Guid.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Undo/UndoWorld.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityTypes.h"
#include "World/Serialization/MapData.h"

namespace Opaax
{
    class World;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo steps of entity actions. Each carries only what its inverse needs (a rename is two
    // strings). Plain structs; building one at the call site is all it takes.
    // =============================================================================

    /**
     * Entities were created. Redo brings them back with their original guids (re-running
     * EntityOps::Create would make new ones).
     */
    struct EntityCreate
    {
        /** What was created, captured after the fact. */
        MapData Entities;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Create Entity"; }
    };

    /**
     * A prefab was placed. Same bodies as EntityCreate; a separate type for its label.
     * The prefab file is not affected by undo.
     */
    struct PrefabInstantiate
    {
        /** The instance's entities, captured after the fact. */
        MapData    Entities;

        /** The level, or the prefab panel placing a nested prefab into its own world. */
        EUndoWorld Scope = EUndoWorld::Active;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Instantiate Prefab"; }
    };

    /**
     * A selection was replaced by an instance of a new prefab: one step for one gesture.
     * Holds both sides (destroyed originals, created instance). Destroy first, then restore, both
     * ways (restoring selects, destroying clears).
     * The prefab file stays on disk after undo (like Unity).
     */
    struct PrefabCreateFromSelection
    {
        /** What was selected, captured before the swap. */
        MapData Originals;

        /** What replaced it (the instance), captured after. */
        MapData Instance;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Create Prefab"; }
    };

    /**
     * Instance entities were reverted to their prefab's values. Both sides are full component
     * records (a revert cannot be inverted by re-running anything). No entity is created or
     * destroyed, so one body serves both directions.
     */
    struct PrefabRevert
    {
        /** The overridden state, captured before. */
        MapData Before;

        /** The prefab's state, captured after. */
        MapData After;

        /**
         * Pieces the revert brought back (deleted from the instance by the author). Restoring Before
         * cannot remove them, so undo destroys this list.
         */
        MapData Created;

        /**
         * Pieces the revert removed (no longer in the prefab). Before holds them, so undo brings them
         * back; redo destroys this list.
         */
        MapData Destroyed;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Revert to Prefab"; }
    };

    /**
     * Entities were destroyed: EntityCreate the other way round. Stores its world (the level and the
     * prefab panel both record it).
     */
    struct EntityDelete
    {
        /** What was destroyed, captured before. */
        MapData    Entities;

        EUndoWorld Scope = EUndoWorld::Active;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Delete Entity"; }
    };

    /**
     * A subtree was moved under another parent (Hierarchy drag, or Detach). One entry per entity:
     * the root's parent and local change, and maps follow a cross-map drop. Written back field by
     * field (SetParent would recompute the local).
     */
    struct EntityReparent
    {
        struct Entry
        {
            Guid               Id;
            Guid               ParentBefore, ParentAfter;
            MapId              MapBefore,    MapAfter;
            TransformComponent LocalBefore,  LocalAfter;
        };

        TDynArray<Entry> Entries;

        EUndoWorld Scope = EUndoWorld::Active;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Reparent"; }
    };

    /** One entity was renamed. */
    struct EntityRename
    {
        Guid        EntityId;
        OpaaxString Before;
        OpaaxString After;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Rename"; }
    };

    /**
     * A gizmo drag moved, rotated or scaled the selection. Any mode, any count: a before and after
     * TransformComponent per entity. A whole drag is one step (Begin on grab, End on release).
     * Stores its world (the level gizmo and the prefab panel both record it).
     */
    struct EntityTransform
    {
        struct Entry
        {
            Guid               Id;
            TransformComponent Before;
            TransformComponent After;
        };

        TDynArray<Entry> Entries;

        /** "Move" / "Rotate" / "Scale", so the menu reads "Undo Move". */
        OpaaxString Name;

        EUndoWorld  Scope = EUndoWorld::Active;

        /** Stores InIds' transforms in InWorld as the before state, dropping any open step. */
        void Begin(World& InWorld, const TDynArray<EntityID>& InIds, const char* InName, EUndoWorld InScope);

        /**
         * Stores them again as the after state.
         * @return True if something moved. A grab without motion is not a step
         */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return Name.CStr(); }
    };
}
