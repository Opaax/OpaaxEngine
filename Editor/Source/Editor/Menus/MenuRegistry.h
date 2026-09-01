#pragma once

#include "Editor/Menus/EditorMenuCategory.h"

namespace Opaax::Editor
{
    /**
     * @class MenuRegistry
     *
     * WHAT sits on the editor's menu bar: an ordered set of root categories, and the storage behind
     * EditorExtensionRegistrar::Menus() (Editor.md D10).
     *
     * The same three parts every other route has — register, consume, Count — so a reader who knows
     * ViewportToolbarRegistry or PanelRegistry already knows this one. It was called `EditorMenu`
     * until 2026-09-01, the only route in the registrar not named as a registry, which made it look
     * like a different kind of thing when it never was (**MR2b**).
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
     *
     * OWNED BY THE GUI, which is what draws it (**MR2e**). The registrar reaches it through a bound
     * route, the WorldSubsystemRoute shape — so `Menus()` reads the same at every call site.
     */
    class MenuRegistry
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        MenuRegistry() = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        // Owns nodes through TUniquePtr and hands out references into them (I6's corollary: an
        // owner of a move-only member must say so, or the implicit copy is instantiated anyway).
        MenuRegistry(const MenuRegistry&)            = delete;
        MenuRegistry& operator=(const MenuRegistry&) = delete;

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
        /**
         * Emit every root category, in bar order.
         *
         * The BAR ITSELF is not opened here — the caller has already opened it, which is what lets
         * the title bar put its window buttons in the same row.
         *
         * CONST like EditorCommandRegistry::Execute: the tree is built before the extension
         * registrar seals, and a draw must not be able to add to it.
         */
        void Draw(EditorContext& InContext) const;

        /** @return How many COMMAND entries exist, at any depth — what the seal log reports. */
        Uint64 Count() const noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TUniquePtr<EditorMenuCategory>> m_Categories;
    };
}
