#pragma once

#include "Core/Tag/OpaaxTag.h"
#include "Editor/Menus/EditorMenuCommandNode.h"
#include "Editor/Menus/IEditorMenuNode.h"

namespace Opaax::Editor
{
    /**
     * @class EditorMenuCategory
     *
     * A menu that opens: "File", or "Tools/Debug". A category IS a node, so a category holds
     * categories and the bar nests as deep as it is written.
     *
     * ONE ordered child list holds categories, commands and separators together, so they interleave
     * and registration order is draw order — the reason a separator can sit between two entries
     * without either of them knowing.
     *
     * Children are TUniquePtr and that is LOAD-BEARING, not style: SubCategory() and AddCommand()
     * hand back a reference the caller keeps and adds to, so storing children by value would leave
     * that reference dangling the moment the next child reallocated the array.
     */
    class EditorMenuCategory final : public IEditorMenuNode
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit EditorMenuCategory(const OpaaxStringID InID, const OpaaxString& InParentPath = OpaaxString())
            : IEditorMenuNode(InID, InParentPath)
        {
        }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** @return The child under InID that is a category / a command, or null. */
        EditorMenuCategory*    FindCategory(OpaaxStringID InID);
        EditorMenuCommandNode* FindCommand(OpaaxStringID InID);

    public:
        /**
         * GET-OR-CREATE the sub-menu named InID.
         *
         * Get-or-create rather than add, because two modules naming the same menu mean the same
         * menu: this is what makes a game module's "Tools" the editor's "Tools" with nobody
         * coordinating. Keyed on the interned id, so the lookup is an integer compare.
         */
        EditorMenuCategory& SubCategory(OpaaxStringID InID);

        /**
         * Add an entry that dispatches InCommand.
         *
         * @param InLabel   What it reads as, and its identity within this category.
         * @param InCommand The tag EditorCommandRegistry answers for. NOT resolved here — an
         *   unregistered tag is reported by the registry at click time, so registration order
         *   between menus and commands never matters.
         * @return The new entry, so SetEnabled / SetChecked chain onto this call. A duplicate label
         *   keeps the FIRST entry and warns, matching EditorCommandRegistry::Register.
         */
        EditorMenuCommandNode& AddCommand(OpaaxStringID InLabel, const OpaaxTag& InCommand);

        /** Add a rule below the entries registered so far. */
        void AddSeparator();

        /** Grey this whole menu — asked every frame, unset means always enabled. */
        EditorMenuCategory& SetEnabled(FMenuPredicate InPredicate);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorMenuNode interface
        void   Draw(EditorContext& InContext) const override;
        Uint64 CountCommands() const noexcept override;

        EditorMenuCategory* AsCategory() noexcept override { return this; }
        //~End IEditorMenuNode interface

        // =============================================================================
        // Getter

        /** The children in registration order — what Draw walks, and what EditorMenu counts. */
        const TDynArray<TUniquePtr<IEditorMenuNode>>& GetChildren() const noexcept { return m_Children; }

        bool IsEmpty() const noexcept { return m_Children.empty(); }

        // End Getter
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TUniquePtr<IEditorMenuNode>> m_Children;
        FMenuPredicate                         m_IsEnabled;
    };
}
