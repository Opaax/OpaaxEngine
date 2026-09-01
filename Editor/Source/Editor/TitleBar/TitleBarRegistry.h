#pragma once

#include "Editor/Menus/EditorMenuCategory.h"

namespace Opaax::Editor
{
    /**
     * @class TitleBarRegistry
     *
     * WHAT a module puts in the editor's title bar, and the storage behind
     * EditorExtensionRegistrar::TitleBar() (Editor.md D10). Menu categories today; the bar is simply
     * where they go, which is why the type is named for the bar rather than for its current content.
     *
     * DATA ONLY — it stores and it counts, and EditorTitleBar is what walks it and draws. That is the
     * PanelRegistry -> EditorPanels pipeline exactly, and it is what makes moving to another UI
     * backend a matter of implementing IEditorGui rather than rewriting anything here.
     *
     * IT IS A TREE, BUILT BY THE CALLER. It used to be a flat list of slash-separated path strings
     * whose nesting was re-derived every frame; a path can carry an ORDER, an enabled predicate or
     * a checked state for nothing, so the moment a category became a thing that holds state,
     * deriving it from a prefix stopped being free (**MR2a**).
     *
     * Bar order is the order Category() was first called, which is authored — the editor's natives
     * register before any module, so a game module can add to the bar without being able to push
     * File out of the way.
     *
     * A node carries a COMMAND TAG, never a closure: clicking is Commands().Execute(tag, context),
     * the same call a key binding makes.
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
        EditorMenuCategory& Category(OpaaxStringID InID);

        // =============================================================================
        // Consume
        // =============================================================================
    public:
        /** The root categories in bar order — what EditorTitleBar walks. */
        const TDynArray<TUniquePtr<EditorMenuCategory>>& Categories() const noexcept { return m_Categories; }

        /** @return How many COMMAND entries exist, at any depth — what the seal log reports. */
        Uint64 Count() const noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TUniquePtr<EditorMenuCategory>> m_Categories;
    };
}
