#pragma once

#include "Core/OpaaxTypes.h"
#include "World/Entity/Entity.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorSelection — WHAT the author has selected. Owned by EditorService, referenced by
    //   EditorContext so panels and drawers read and write it without a locator. The Hierarchy and
    //   the viewport both write; the Inspector and the viewport's outline read.
    //
    //   A SET, with a PRIMARY. Get() answers the primary — the last entity touched — which is what
    //   lets the Inspector keep drawing exactly one entity while everything else grows a multi-
    //   selection around it: multi-SELECT is this block's job, multi-EDIT is not, and keeping the
    //   old accessor honest is what holds that line.
    //
    //   Stored as EntityID plus one World*, not as Entity handles: every member of a selection is
    //   in the same world by construction (selecting in another world replaces the set), so one
    //   pointer is the whole difference and there is no way for two entries to disagree about it.
    //   Entity holds a raw World*, so this is invalidated from OUTSIDE — EditorService subscribes
    //   to WorldManager and retargets every entry by GUID, or clears, on each world change.
    // =============================================================================
    class EditorSelection
    {
        // =============================================================================
        // Write
        // =============================================================================
    public:
        /** Replace the whole selection with one entity. A plain click, and every pre-② caller. */
        void Select(Entity InEntity)
        {
            Clear();
            Add(InEntity);
        }

        /** Add without dropping what is already selected — a Ctrl+drag marquee. */
        void Add(Entity InEntity)
        {
            if (!InEntity.IsValid() || Contains(InEntity)) { return; }

            m_World = InEntity.GetWorld();
            m_Ids.emplace_back(InEntity.GetHandle());
        }

        /** In if out, out if in — Ctrl+click. Removing the primary promotes whatever is left. */
        void Toggle(Entity InEntity)
        {
            if (!InEntity.IsValid()) { return; }

            for (Uint64 lIndex = 0; lIndex < m_Ids.size(); ++lIndex)
            {
                if (m_Ids[lIndex] != InEntity.GetHandle()) { continue; }

                m_Ids.erase(m_Ids.begin() + static_cast<std::ptrdiff_t>(lIndex));
                if (m_Ids.empty()) { m_World = nullptr; }
                return;
            }

            Add(InEntity);
        }

        /** Replace with a whole list — a marquee's ordinary outcome. Skips invalid handles. */
        void Replace(World* InWorld, const TDynArray<EntityID>& InIds)
        {
            Clear();

            if (InWorld == nullptr) { return; }

            for (const EntityID lId : InIds)
            {
                Add(Entity{ lId, InWorld });
            }
        }

        void Clear() noexcept
        {
            m_Ids.clear();
            m_World = nullptr;
        }

        // =============================================================================
        // Read
        // =============================================================================
    public:
        /**
         * The PRIMARY — the last entity added — or an invalid Entity when nothing is selected.
         * The Inspector draws this one and only this one.
         */
        Entity Get() const noexcept
        {
            return m_Ids.empty() ? Entity{} : Entity{ m_Ids.back(), m_World };
        }

        bool HasSelection() const noexcept { return !m_Ids.empty(); }

        bool Contains(Entity InEntity) const noexcept
        {
            if (m_World == nullptr || InEntity.GetWorld() != m_World) { return false; }

            for (const EntityID lId : m_Ids)
            {
                if (lId == InEntity.GetHandle()) { return true; }
            }

            return false;
        }

        /** Every selected handle, in selection order. The outline and focus-selected walk this. */
        const TDynArray<EntityID>& Ids() const noexcept { return m_Ids; }

        Uint64 Count() const noexcept { return m_Ids.size(); }

        /** The world every entry belongs to, or null when empty. */
        World* GetWorld() const noexcept { return m_World; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<EntityID> m_Ids;
        World*              m_World = nullptr;
    };
}
