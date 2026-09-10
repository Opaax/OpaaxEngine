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
    // The steps an ENTITY verb records. Each carries exactly what its own inverse needs and
    // nothing more, so a rename costs two strings rather than a snapshot of anything.
    //
    // Plain aggregates with three members — the TransformDelta/ComponentData shape. Building one
    // at the call site IS the whole opt-in; there is no concept to declare and no label to
    // register (**UN1**).
    // =============================================================================

    /**
     * Entities were created.
     *
     * Redo brings them back ON THEIR ORIGINAL GUIDS, which is what every inter-entity reference
     * stands on — and the reason a redo is never a second Execute: `EntityOps::Create` mints a
     * fresh Guid and a freshly uniquified name, so re-running it would produce different entities.
     */
    struct EntityCreate
    {
        /** What was created, captured after the fact. Its `Id` stays invalid — this is not a map. */
        MapData Entities;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Create Entity"; }
    };

    /**
     * A prefab was instantiated (⑦-C P1b).
     *
     * EntityCreate's two bodies verbatim — an instance is entities, and undoing one is destroying
     * them. It exists for its LABEL: "Undo Create Entity" after placing a prefab names the wrong
     * verb, and the Edit menu shows that text.
     *
     * THE FILE IS NOT PART OF THIS. Undo takes back the placement, never the `.opaaxprefab` on
     * disk (⑦-C **K6**, matching Unity) — a step that deleted an asset would be the one undo
     * nobody expects.
     */
    struct PrefabInstantiate
    {
        /** The instance's entities, captured after the fact — EntityCreate's rule. */
        MapData Entities;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Instantiate Prefab"; }
    };

    /**
     * A selection BECAME an instance of a new prefab (⑦-C P2) — one step for a swap, because it
     * was one gesture.
     *
     * Two payloads because the edit destroyed one set of entities and created another, and undo has
     * to put back exactly what was there. Recording it as an `EntityDelete` plus an
     * `EntityInstantiate` would need TWO Ctrl+Z for one action, which is the classic way a
     * composite verb ends up feeling broken.
     *
     * ORDER IS LOAD-BEARING in both directions: destroy first, restore second. `RestoreEntities`
     * SELECTS what it brought back and `DestroyEntities` clears the selection, so doing them the
     * other way round would leave nothing selected after an undo.
     *
     * THE FILE IS NOT PART OF THIS (⑦-C **K6**). Undo puts the original entities back and takes the
     * instance away; the `.opaaxprefab` it wrote stays on disk, exactly as Unity leaves the asset.
     * The prefab can be placed again from the browser, or deleted in the file system.
     */
    struct PrefabCreateFromSelection
    {
        /** What was selected, captured BEFORE the swap: nothing else can recover it. */
        MapData Originals;

        /** What replaced it, captured after — the instance's derived guids. */
        MapData Instance;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Create Prefab"; }
    };

    /**
     * Instance entities were put back to their prefab's values (⑦-C P3).
     *
     * Both sides are full component records rather than a hand-written inverse, for **UN4**'s
     * reason: a revert is not exactly invertible by re-running anything — it removes components the
     * template does not have and restores ones the instance had deleted, so only "be this again"
     * in each direction is honest.
     *
     * No entity is created or destroyed here, which is why one type serves both directions with
     * the same body: the guids are untouched throughout.
     */
    struct PrefabRevert
    {
        /** The overridden state, captured before. */
        MapData Before;

        /** The prefab's state, captured after. */
        MapData After;

        /**
         * Pieces the revert BROUGHT BACK — entities the author had deleted from the instance.
         *
         * They need their own list because `MapFactory::Restore` leaves entities it does not name
         * alone, so restoring `Before` cannot remove them: nothing in `Before` can express "and
         * this one should not exist", since it was captured when it did not.
         */
        MapData Created;

        /**
         * Pieces the revert TOOK AWAY — instance entities whose template the prefab no longer has
         * (the prefab REMOVED a piece; L87's other half). `Before` holds them too, so undo brings
         * them back by restoring it; this list is what redo destroys, since `After` never named them.
         */
        MapData Destroyed;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Revert to Prefab"; }
    };

    /**
     * Entities were destroyed — the same two bodies as EntityCreate, the other way round.
     * Names its world (P8 V4): the level's Delete and the prefab panel's record the same type.
     */
    struct EntityDelete
    {
        /** What was destroyed, captured BEFORE the fact: nothing else can recover it. */
        MapData    Entities;

        EUndoWorld Scope = EUndoWorld::Active;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Delete Entity"; }
    };

    /** One entity was renamed. Nothing to serialize — a name is its own inverse. */
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
     * A gizmo drag moved, turned or scaled the selection.
     *
     * ONE TYPE FOR ALL THREE MODES AND ANY COUNT. The payload is a before and an after
     * TransformComponent per entity, which is the same three fields whichever handle was grabbed —
     * so the mode is only the NAME, and one entity is a list of one. Splitting it per mode or per
     * cardinality would be four types with identical bodies.
     *
     * THE ONLY STEP THAT SERIALIZES NOTHING AT ALL: it copies a POD component, twice.
     *
     * A whole drag is ONE of these because the panel owns it across frames — Begin on the grab,
     * End on the release. The per-frame TransformSelectedCommand records nothing.
     *
     * NAMES ITS WORLD (P8 V3): the level's gizmo and the prefab panel's record the same type, and
     * Scope is what makes Undo find the right entities — by guid, in that world.
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

        /** "Move" / "Rotate" / "Scale" — the mode's own name, so the menu reads "Undo Move". */
        OpaaxString Name;

        EUndoWorld  Scope = EUndoWorld::Active;

        /** Cache InIds' transforms in InWorld as the BEFORE half, dropping any step left open. */
        void Begin(World& InWorld, const TDynArray<EntityID>& InIds, const char* InName, EUndoWorld InScope);

        /**
         * Cache them again as the AFTER half.
         *
         * @return true when something actually moved. A grab with no motion, a drag that ended
         *   where it started, and a closed step nobody opened all answer false — none is a step.
         */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return Name.CStr(); }
    };
}
