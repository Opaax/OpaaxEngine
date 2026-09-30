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

        // Owns nodes through TUniquePtr and hands out references into them (non-copyable).
        TitleBarRegistry(const TitleBarRegistry&)            = delete;
        TitleBarRegistry& operator=(const TitleBarRegistry&) = delete;

        // =============================================================================
        // Register
        // =============================================================================
    public:
        /**
         * Gets or creates a root category ("File", "Level", "Tools").
         * @return The category, so entries chain off it. Creation order is the bar order.
         */
        EditorTitleBarCategory& Category(OpaaxStringID InID);

        // =============================================================================
        // Consume
        // =============================================================================
    public:
        /** The root categories, in bar order. */
        const TDynArray<TUniquePtr<EditorTitleBarCategory>>& Categories() const noexcept { return m_Categories; }

        /** @return How many command entries exist, at any depth. */
        Uint64 Count() const noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TUniquePtr<EditorTitleBarCategory>> m_Categories;
    };
}
