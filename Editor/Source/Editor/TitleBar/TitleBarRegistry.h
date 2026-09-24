#pragma once

#include "Editor/Menus/EditorTitleBarCategory.h"

namespace Opaax::Editor
{
    /**
     * @class TitleBarRegistry
     */
    class TitleBarRegistry
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        TitleBarRegistry() = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        // Owns nodes through TUniquePtr and hands out references into them (I6's corollary: an
        // owner of a move-only member must say so, or the implicit copy is instantiated anyway).
        TitleBarRegistry(const TitleBarRegistry&)            = delete;
        TitleBarRegistry& operator=(const TitleBarRegistry&) = delete;

        // =============================================================================
        // Register
        // =============================================================================
    public:
        /**
         * GET-OR-CREATE a root category — "File", "Level", "Tools".
         *
         * @return The category, so entries chain off it. Creation order is left-to-right bar order.
         */
        EditorTitleBarCategory& Category(OpaaxStringID InID);

        // =============================================================================
        // Consume
        // =============================================================================
    public:
        /** The root categories in bar order — what EditorTitleBar walks. */
        const TDynArray<TUniquePtr<EditorTitleBarCategory>>& Categories() const noexcept { return m_Categories; }

        /** @return How many COMMAND entries exist, at any depth — what the seal log reports. */
        Uint64 Count() const noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TUniquePtr<EditorTitleBarCategory>> m_Categories;
    };
}
