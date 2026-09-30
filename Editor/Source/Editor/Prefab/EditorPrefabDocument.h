#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"   // Vector2F
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
    // EditorPrefabDocument — which .opaaxprefab is being edited, and in which world.
    //   Holds its own copy, as an entity world only this panel sees (the ResourceManager's copy is
    //   what placed instances were built from).
    //   The world is created once and kept while the editor runs: destroying an Edit world clears the
    //   level's undo history, so closing this panel must not destroy one.
    //   Entities keep the prefab's own guids (they are the templates).
    //   Also owns its undo history and selection (steps recorded here use EUndoWorld::Prefab). All
    //   three are cleared when a new prefab is opened.
    // =============================================================================
    class EditorPrefabDocument
    {
        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Loads InAbsPath and rebuilds the editing world around it (cleared first).
         * @return False when the file did not load; the previous document then stays open
         */
        bool Open(EditorContext& InContext, const OpaaxString& InAbsPath);

        /**
         * Writes the editing world back to the file, then announces it: ResourceOps::SavedToDisk reloads
         * the resource and every placement in the level is updated, keeping its overrides.
         */
        bool Save(EditorContext& InContext);

        /**
         * Writes InAbsPath as a variant of the open prefab (one record placing this file, with the
         * current differences as overrides) and opens it. The base file is unchanged. Refused onto the
         * open file itself or outside the asset folders.
         */
        bool SaveAsVariant(EditorContext& InContext, const OpaaxString& InAbsPath);

        /**
         * Places one instance of the prefab at InAssetPath into the editing world (a nested placement).
         * Selected and recorded on this document's stack; Save folds it back to a record. Refused when
         * the prefab would contain itself (directly or at any depth).
         * @param InAtWorld Where the anchor lands, or null for the prefab's authored positions
         * @param InParent  A row it was dropped on: the roots hang under it with the authored pose as
         *   their local. ENTITY_NONE places at root
         * @return How many entities were created. 0 = refused (the log says why)
         */
        Uint64 Place(EditorContext& InContext, const OpaaxString& InAssetPath, const Vector2F* InAtWorld,
                     EntityID InParent = ENTITY_NONE);

        /** Forgets the document. The world is kept (see the class note). */
        void Close();

        // =========================================================================
        // Get - Set
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** The world holding the prefab's entities, or null before anything was opened. */
        World*             GetWorld() const noexcept { return m_World; }

        /**
         * Bumped by every Open and Close (whenever the entities were replaced). Compare against it when
         * holding entity handles: entt reuses handles.
         */
        Uint64             Generation() const noexcept { return m_Generation; }

        /** This document's own undo history. */
        EditorUndo&        Undo() noexcept { return m_Undo; }

        /** The selection in this world (not the context's). */
        EditorSelection&   Selection() noexcept { return m_Selection; }

        /**
         * Whether the editing world differs from what was last written. Derived, and cached against the
         * world's revision, so calling it every frame is cheap.
         */
        bool IsDirty(EditorContext& InContext) const;

        // End Get - Set
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        /** The prefab's entities as text, as last read or written (IsDirty compares against it). */
        OpaaxString m_Baseline;

        OpaaxString m_AbsPath;

        /** Non-owning: WorldManager owns it. Created on the first Open and kept. */
        World*      m_World = nullptr;

        Uint64      m_Generation = 0;

        EditorUndo      m_Undo;
        EditorSelection m_Selection;

        /** IsDirty's cache, keyed by the world's revision. */
        mutable Uint64 m_LastRevision = ~0ull;
        mutable bool   m_bDirty       = false;
    };
}
