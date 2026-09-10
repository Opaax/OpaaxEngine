#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;
}

namespace Opaax::Editor
{
    struct EditorContext;

    inline constexpr LogCategory LogEditorPrefabDocument{"EditorPrefabDocument"};

    // =============================================================================
    // EditorPrefabDocument — WHICH `.opaaxprefab` is being edited, and in which world (⑦-C P6).
    //
    //   IT OWNS ITS DATA THE WAY EVERY OTHER DOCUMENT DOES (**SS4**/**AN8**), and here that means
    //   owning a WORLD: the copy in the `ResourceManager` is what every placed instance was built
    //   from, so editing that one would change the level under the author's hands and lose the work
    //   on the next reload. The prefab's entities are instantiated into a world of their own, and
    //   nothing outside this panel can see them.
    //
    //   THE WORLD IS CREATED ONCE AND NEVER DESTROYED while the editor runs, which is a decision
    //   rather than laziness. `EditorService::HandleWorldDestroyed` clears the undo history whenever
    //   an **Edit**-mode world dies (**UN1**) — correct while the only Edit world was the level's,
    //   and a trap the moment a second one exists: closing the prefab panel would have wiped the
    //   level's history. Reusing one world sidesteps it entirely, where a third `EWorldMode` would
    //   have meant auditing 25 comparisons for "did this mean Edit-and-not-Preview".
    //
    //   The entities keep the prefab's OWN guids — they are the templates, so nothing is derived
    //   here (**K2** applies to placements, not to the prefab itself).
    //
    //   IT OWNS ITS HISTORY TOO (P8 V3), AND ITS SELECTION (V4). The level's stack is the level's
    //   (**UN1**); a step recorded here names this document's world (EUndoWorld::Prefab) and is
    //   reached by the panel's declared Undo/Redo commands, Ctrl+S's exact shape. The selection is
    //   here rather than on the panel because a step's replay touches it — a restore re-selects,
    //   a destroy clears — and a step can reach a document, never a panel. All three are cleared
    //   with the entities: a step or a handle from before an Open names nothing.
    // =============================================================================
    class EditorPrefabDocument
    {
        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Load InAbsPath and rebuild the editing world around it.
         *
         * The world is CLEARED first, so opening a second prefab cannot leave the first one's
         * entities behind — one document, one world, one prefab at a time.
         *
         * @return false when the file did not load; the previous document is then left open, for
         *   MP3's reason: a failed read must not half-replace what the author already had.
         */
        bool Open(EditorContext& InContext, const OpaaxString& InAbsPath);

        /**
         * Capture the editing world back into the file, then ANNOUNCE it (P4).
         *
         * The announce is the whole point of editing a prefab: `ResourceOps::SavedToDisk` reloads
         * the resource and the reconciler re-applies it to every placement in the level, each
         * keeping its own overrides. Until P6 that seam had no natural trigger.
         */
        bool Save(EditorContext& InContext);

        /** Forget the document. The world is kept — see the class note. */
        void Close();

        // =========================================================================
        // Get - Set
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** The world the prefab's entities live in, or null before anything was opened. */
        World*             GetWorld() const noexcept { return m_World; }

        /**
         * Bumped by every Open and Close — i.e. every time the world's entities were REPLACED.
         * A reader holding entity handles compares against it: entt reuses handles, so a handle
         * that survived a rebuild would silently name a different entity (**MV4**).
         */
        Uint64             Generation() const noexcept { return m_Generation; }

        /** This document's own history — see the class note. */
        EditorUndo&        Undo() noexcept { return m_Undo; }

        /** What is selected in THIS world — never the context's (entt reuses handles across worlds, **MV4**). */
        EditorSelection&   Selection() noexcept { return m_Selection; }

        /**
         * Does the editing world differ from what was last written?
         *
         * DERIVED, never tracked — **MP5**'s rule, and for its reason: there is nothing to hook,
         * because every edit goes through a drawer whose contract says "was it drawn", not "was it
         * changed". Cached against the world's revision, so a per-frame caller costs nothing
         * between edits (the level document's gate).
         */
        bool IsDirty(EditorContext& InContext) const;

        // End Get - Set
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        /** The prefab's entities as TEXT, as last read or written — IsDirty compares against it. */
        OpaaxString m_Baseline;

        OpaaxString m_AbsPath;

        /** Non-owning: WorldManager owns it. Created on the first Open and kept (see the note). */
        World*      m_World = nullptr;

        Uint64      m_Generation = 0;

        EditorUndo      m_Undo;
        EditorSelection m_Selection;

        /** IsDirty's cache, keyed by the world's revision — recomputed only when it moved. */
        mutable Uint64 m_LastRevision = ~0ull;
        mutable bool   m_bDirty       = false;
    };
}
