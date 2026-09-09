#pragma once

#include "Application/Services/ILogger.h"

#include "Editor/Resources/EditorResourceEvents.h"

#include "World/Serialization/MapData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    inline constexpr LogCategory LogPrefabReconciler{"PrefabReconciler"};

    // =============================================================================
    // PrefabReconciler — editing a prefab updates the instances already in the world (⑦-C P4).
    //
    //   THE ONE RESOURCE TYPE A RELOAD CANNOT SERVE. `ResourceManager::Reload` swaps a payload in
    //   place, so every live `ResourceRef` sees the new texture, sheet or clip for free. A prefab's
    //   instances are not refs — they were MATERIALIZED into entities the moment they were placed —
    //   so the world keeps whatever the old prefab said until something rebuilds them. This is that
    //   something.
    //
    //   IT BRACKETS THE RELOAD, and that is forced rather than chosen. An instance keeps its
    //   overrides across the edit, and an override is only computable against the template the
    //   instance was built from — which stops existing the instant the payload is swapped. So:
    //
    //     OnSaving  — FOLD the affected placements against the OLD prefab. The records that come
    //                 out hold exactly the deviations the author made, and nothing else.
    //     OnSaved   — EXPAND them against the NEW prefab and Restore. Every property nobody
    //                 overrode follows the edit; every property they did override survives it.
    //
    //   Both halves are `PrefabFold`'s, unchanged — the same pair a map save and a map load use.
    //   Nothing prefab-specific happens here that a file round trip does not already do; the only
    //   new idea is doing it in memory, around a reload, instead of around a file.
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
        /** Bind both phases. Called once by EditorService. */
        void Bind(EditorResourceEvents& InEvents);

        /** Unbind. Both by owner, so a torn-down reconciler cannot be called. */
        void Unbind(EditorResourceEvents& InEvents);

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** Fold the placements of the saved prefab, while the OLD payload is still resident. */
        void HandleSaving(const ResourceSavedEvent& InEvent);

        /** Rebuild them against the new payload. */
        void HandleSaved(const ResourceSavedEvent& InEvent);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EditorContext& m_Context;

        /**
         * The folded placements, held between the two phases of ONE save.
         *
         * Only ever non-empty inside a `ResourceOps::SavedToDisk` call, which broadcasts both
         * phases synchronously — so this is a parameter that could not be passed, not state with a
         * lifetime. Cleared by HandleSaved unconditionally, including when it rebuilds nothing.
         */
        MapData m_Pending;
    };
}
