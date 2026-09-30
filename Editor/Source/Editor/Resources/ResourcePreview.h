#pragma once

#include "Core/OpaaxTypes.h"                 // TDynArray, Uint32, Uint64
#include "Editor/Resources/ResourceScan.h"   // ResourceFile, held by value

namespace Opaax::Editor
{
    // =============================================================================
    // ResourcePreviewEntry — one thing being looked at.
    // =============================================================================
    struct ResourcePreviewEntry
    {
        ResourceFile File;
        Uint32       TypeId = 0;   // ResourceTypeID::Get<T>(), resolved by the browser
    };

    // =============================================================================
    // ResourcePreview — which resources are open in the Preview panel, in opening order. Lives outside
    //   the panel so a resource type's activate callback (which only gets EditorContext) can reach it.
    //   A list, so two images can be compared. Opening an already open path focuses it instead.
    //   Stores what was asked for, not the loaded resource (the panel holds the loads).
    // =============================================================================
    class ResourcePreview
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Opens InFile, of resource type InTypeId, and focuses it. An already open path is only focused.
         */
        void Open(const ResourceFile& InFile, Uint32 InTypeId)
        {
            for (Uint64 i = 0; i < m_Entries.size(); ++i)
            {
                if (m_Entries[i].File.AbsPath == InFile.AbsPath)
                {
                    m_Focused = i;
                    return;
                }
            }

            m_Entries.emplace_back(ResourcePreviewEntry{ InFile, InTypeId });
            m_Focused = m_Entries.size() - 1;
        }

        /** Closes the entry at InIndex. Does nothing when out of range. */
        void Close(Uint64 InIndex)
        {
            if (InIndex >= m_Entries.size())
            {
                return;
            }

            m_Entries.erase(m_Entries.begin() + static_cast<Int64>(InIndex));

            // Clamped rather than cleared: closing one of several keeps a valid focus.
            if (m_Focused >= m_Entries.size() && !m_Entries.empty())
            {
                m_Focused = m_Entries.size() - 1;
            }
        }

        void CloseAll() { m_Entries.clear(); m_Focused = 0; }

        // =============================================================================
        // Get - Set
    public:
        const TDynArray<ResourcePreviewEntry>& Entries() const noexcept { return m_Entries; }

        /** @return The entry the panel should scroll to / open. Meaningless while IsEmpty(). */
        Uint64 Focused() const noexcept { return m_Focused; }

        bool IsEmpty() const noexcept { return m_Entries.empty(); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<ResourcePreviewEntry> m_Entries;
        Uint64                          m_Focused = 0;
    };
}
