// EntityUndoables.h
#pragma once

#include "Core/GUID/Guid.h"
#include "Core/String/OpaaxString.hpp"
#include "World/Components/TransformComponent.h"
#include "World/Serialization/MapData.h"

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

    /** Entities were destroyed — the same two bodies as EntityCreate, the other way round. */
    struct EntityDelete
    {
        /** What was destroyed, captured BEFORE the fact: nothing else can recover it. */
        MapData Entities;

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

        /** Cache the selection's transforms as the BEFORE half, dropping any step left open. */
        void Begin(const EditorContext& InContext, const char* InName);

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
