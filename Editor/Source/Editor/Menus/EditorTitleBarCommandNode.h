#pragma once

#include "Core/Tag/OpaaxTag.h"
#include "Editor/Commands/EditorCommandParams.h"
#include "Editor/Menus/IEditorTitleBarNode.h"

namespace Opaax::Editor
{
    /**
     * @class EditorTitleBarCommandNode
     *
     * A clickable entry. It carries a COMMAND TAG and nothing else — clicking it is
     * `Commands().Execute(tag, context)`, the identical call a key binding will make, which is what
     * lets the menu and a shortcut trigger one verb rather than two copies of it. There is
     * deliberately no closure form: behaviour lives in an EditorCommand, never in the bar.
     *
     * The facets are all OPTIONAL, so what the entry IS follows from which of them were set — the
     * same "optional, detected, defaulted" shape WS2's ShouldCreate uses, rather than a kind enum
     * that has to agree with the fields beside it. Two are predicates asked every frame
     * (SetEnabled, SetChecked); SetParams is the payload, which is why AddCommand did not grow an
     * overload for it.
     */
    class EditorTitleBarCommandNode final : public IEditorTitleBarNode
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorTitleBarCommandNode(OpaaxStringID InLabel, const OpaaxString& InParentPath, const OpaaxTag& InCommand)
            : IEditorTitleBarNode(InLabel, InParentPath)
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
        EditorTitleBarCommandNode& SetEnabled(FMenuPredicate InPredicate);

        /**
         * Draw the entry as a CHECKABLE item reading InPredicate for its tick.
         *
         * Setting it is what makes the entry checkable at all — the state stays wherever it really
         * lives, so the tick cannot drift from it.
         */
        EditorTitleBarCommandNode& SetChecked(FMenuPredicate InPredicate);

        /**
         * The payload this entry dispatches with. Unset means NoParams.
         *
         * Only for PLAIN DATA a key binding could also carry — an interned id, a number, a path.
         * A payload only the composition root can resolve (a `Window*`) makes the command
         * menu-only, which is the whole reason QuitCommand takes nothing and reads
         * EditorContext::MainWindow instead.
         */
        template<typename TParams>
        EditorTitleBarCommandNode& SetParams(TParams InParams)
        {
            m_Params = MakeUnique<EditorCommandParamsBox<TParams>>(Move(InParams));
            return *this;
        }

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorMenuNode interface
        void   Draw(EditorContext& InContext) const override;
        Uint64 CountCommands() const noexcept override { return 1; }

        EditorTitleBarCommandNode* AsCommand() noexcept override { return this; }
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

        // Null means NoParams — the shape every entry had before SetParams existed.
        TUniquePtr<IEditorCommandParams> m_Params;
    };
}
