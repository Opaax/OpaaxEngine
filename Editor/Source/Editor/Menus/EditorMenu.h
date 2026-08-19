#pragma once

#include "Editor/Menus/EditorMenuCategory.h"

namespace Opaax::Editor
{
    /**
     * @class EditorMenu
     *
     * The editor's top bar: an ordered set of root categories, and the storage behind
     * EditorExtensionRegistrar::Menus() (Editor.md D10).
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
    class EditorMenu
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorMenu() = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        // Owns nodes through TUniquePtr and hands out references into them (I6's corollary: an
        // owner of a move-only member must say so, or the implicit copy is instantiated anyway).
        EditorMenu(const EditorMenu&)            = delete;
        EditorMenu& operator=(const EditorMenu&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * GET-OR-CREATE a root category — "File", "Level", "Tools".
         *
         * @return The category, so entries chain off it. Creation order is left-to-right bar order.
         */
        EditorMenuCategory& Category(OpaaxStringID InID);

        /**
         * Emit the whole bar.
         *
         * CONST like EditorCommandRegistry::Execute: the tree is built before the extension
         * registrar seals, and a draw must not be able to add to it.
         */
        void Draw(EditorContext& InContext) const;

        // =============================================================================
        // Getter

        /** @return How many COMMAND entries exist, at any depth — what the seal log reports. */
        Uint64 Count() const noexcept;

        // End Getter
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TUniquePtr<EditorMenuCategory>> m_Categories;
    };
}
