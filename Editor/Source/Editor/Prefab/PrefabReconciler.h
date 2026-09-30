#pragma once

#include "Core/Log/Logger.h"

#include "Editor/Resources/EditorResourceEvents.h"

#include "World/Serialization/MapData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    inline constexpr LogCategory LogPrefabReconciler{"PrefabReconciler"};

    // =============================================================================
    // PrefabReconciler — saving a prefab updates the instances already in the world. A reload is not
    //   enough: instances are entities, not refs. It brackets the reload:
    //     OnSaving — fold the affected placements against the old prefab (records = the author's
    //                changes only).
    //     OnSaved  — expand them against the new prefab and Restore: unchanged properties follow the
    //                edit, overridden ones are kept.
    //   Both halves are PrefabFold's, as used by map save and load.
    // =============================================================================
    class PrefabReconciler
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        explicit PrefabReconciler(EditorContext& InContext) noexcept : m_Context(InContext) {}

        PrefabReconciler(const PrefabReconciler&)            = delete;
        PrefabReconciler& operator=(const PrefabReconciler&) = delete;

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /** Binds both phases. Called once by EditorService. */
        void Bind(EditorResourceEvents& InEvents);

        /** Unbinds both. */
        void Unbind(EditorResourceEvents& InEvents);

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** Folds the placements of the saved prefab, while the old data is still loaded. */
        void HandleSaving(const ResourceSavedEvent& InEvent);

        /** Rebuilds them against the new data. */
        void HandleSaved(const ResourceSavedEvent& InEvent);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EditorContext& m_Context;

        /**
         * The folded placements, held between the two phases of one save (both run synchronously inside
         * ResourceOps::SavedToDisk). Cleared by HandleSaved.
         */
        MapData m_Pending;

        /**
         * Every instance entity the fold consumed, by guid. HandleSaved uses it to find entities whose
         * template the prefab dropped.
         */
        TDynArray<Guid> m_Affected;
    };
}
