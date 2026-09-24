#pragma once

#include "Core/OpaaxTypes.h"                 // TDynArray, Uint32, Uint64
#include "Editor/Resources/ResourceScan.h"   // ResourceFile — held BY VALUE

namespace Opaax::Editor
{
    // =============================================================================
    // ResourcePreviewEntry — one thing being looked at.
    // =============================================================================
    struct ResourcePreviewEntry
    {
        ResourceFile File;
        Uint32       TypeId = 0;   // ResourceTypeID::Get<T>() — resolved by the browser, not re-derived
    };

    // =============================================================================
    // ResourcePreview — WHICH resources are open for viewing, in the order they were opened.
    //
    //   Here rather than inside the Preview panel for the reason PIE, EditorSelection and
    //   EditorMapDocument are: the WRITER and the READER are different objects. A type's activate
    //   closure receives an EditorContext and nothing else — there is no typed getter for a live
    //   panel, deliberately (EditorPanels owns panels as IEditorPanel) — so this is the one place
    //   the two can meet.
    //
    //   A LIST, not one entry: comparing two images means having both on screen, which is the whole
    //   reason to open a second one. Opening a path that is already open FOCUSES it instead of
    //   duplicating it, so the list cannot grow by double-clicking the same file.
    //
    //   It stores WHAT was asked for, never the loaded resource: the panel owns the claims, so the
    //   pixels' lifetime stays with the thing that draws them.
    // =============================================================================
    class ResourcePreview
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Open InFile, of resource type InTypeId, and focus it.
         *
         * Re-opening an already-open path only moves the focus — the same file cannot appear twice.
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

        /** Close the entry at InIndex. Out of range is a no-op. */
        void Close(Uint64 InIndex)
        {
            if (InIndex >= m_Entries.size())
            {
                return;
            }

            m_Entries.erase(m_Entries.begin() + static_cast<Int64>(InIndex));

            // Clamped rather than cleared: closing one of several should leave the rest focused
            // somewhere valid, not collapse the whole panel to its empty state.
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

        /** @return Which entry the panel should scroll to / open. Meaningless while IsEmpty(). */
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
