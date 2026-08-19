#pragma once

#include "Core/Tag/OpaaxTag.h"
#include "Editor/Menus/IEditorMenuNode.h"

namespace Opaax::Editor
{
    /**
     * @class EditorMenuCommandNode
     *
     * A clickable entry. It carries a COMMAND TAG and nothing else — clicking it is
     * `Commands().Execute(tag, context)`, the identical call a key binding will make, which is what
     * lets the menu and a shortcut trigger one verb rather than two copies of it. There is
     * deliberately no closure form: behaviour lives in an EditorCommand, never in the bar.
     *
     * The two facets are optional predicates (FMenuPredicate), so what the entry IS follows from
     * which of them were set — the same "optional, detected, defaulted" shape WS2's ShouldCreate
     * uses, rather than a kind enum that has to agree with the fields beside it.
     */
    class EditorMenuCommandNode final : public IEditorMenuNode
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorMenuCommandNode(OpaaxStringID InLabel, const OpaaxString& InParentPath, const OpaaxTag& InCommand)
            : IEditorMenuNode(InLabel, InParentPath)
            , m_Command(InCommand)
        {
        }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Grey the entry out instead of letting it be clicked and refused (**MP7**).
         *
         * @param InPredicate Asked every frame. Unset means always enabled.
         * @return this, so facets chain onto the AddCommand call that made the node.
         */
        EditorMenuCommandNode& SetEnabled(FMenuPredicate InPredicate);

        /**
         * Draw the entry as a CHECKABLE item reading InPredicate for its tick.
         *
         * Setting it is what makes the entry checkable at all — the state stays wherever it really
         * lives, so the tick cannot drift from it.
         */
        EditorMenuCommandNode& SetChecked(FMenuPredicate InPredicate);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorMenuNode interface
        void   Draw(EditorContext& InContext) const override;
        Uint64 CountCommands() const noexcept override { return 1; }

        EditorMenuCommandNode* AsCommand() noexcept override { return this; }
        //~End IEditorMenuNode interface

        // =============================================================================
        // Getter

        const OpaaxTag& GetCommand() const noexcept { return m_Command; }

        // End Getter
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxTag       m_Command;
        FMenuPredicate m_IsEnabled;
        FMenuPredicate m_IsChecked;
    };
}
