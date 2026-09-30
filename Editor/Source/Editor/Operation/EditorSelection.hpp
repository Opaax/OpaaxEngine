#pragma once

#include "Core/OpaaxTypes.h"
#include "World/Entity/Entity.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorSelection — what the author has selected. Owned by EditorService, referenced by
    //   EditorContext. The Hierarchy and the viewport write it; the Inspector and the outline read it.
    //   A set with a primary (the last entity touched, what the Inspector shows).
    //   Stored as EntityIDs plus one World* (all entries are in the same world). EditorService
    //   retargets every entry by guid, or clears, when the world changes.
    // =============================================================================
    class EditorSelection
    {
        // =============================================================================
        // Write
        // =============================================================================
    public:
        /** Replaces the selection with one entity (a plain click). */
        void Select(Entity InEntity)
        {
            Clear();
            Add(InEntity);
        }

        /** Adds without dropping the current selection (Ctrl+drag marquee). */
        void Add(Entity InEntity)
        {
            if (!InEntity.IsValid() || Contains(InEntity)) { return; }

            m_World = InEntity.GetWorld();
            m_Ids.emplace_back(InEntity.GetHandle());
        }

        /** Toggles one entity (Ctrl+click). Removing the primary promotes the last remaining one. */
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

        /** Replaces with a list (a marquee). Skips invalid handles. */
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
         * The primary (last added), or an invalid Entity when nothing is selected. The Inspector shows it.
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

        /** Every selected handle, in selection order. */
        const TDynArray<EntityID>& Ids() const noexcept { return m_Ids; }

        Uint64 Count() const noexcept { return m_Ids.size(); }

        /** The world of the selection, or null when empty. */
        World* GetWorld() const noexcept { return m_World; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<EntityID> m_Ids;
        World*              m_World = nullptr;
    };
}
