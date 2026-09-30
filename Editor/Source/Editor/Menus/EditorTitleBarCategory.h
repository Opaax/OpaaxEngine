#pragma once

#include "Core/Tag/OpaaxTag.h"
#include "Editor/Menus/EditorTitleBarCommandNode.h"
#include "Editor/Menus/IEditorTitleBarNode.h"

namespace Opaax::Editor
{
    /**
     * @class EditorTitleBarCategory
     *
     * A menu that opens: "File", or "Tools/Debug". Categories can nest.
     * One ordered child list holds categories, commands and separators, so registration order is draw
     * order. Children are TUniquePtr because SubCategory() and AddCommand() return references the
     * caller keeps.
     */
    class EditorTitleBarCategory final : public IEditorTitleBarNode
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit EditorTitleBarCategory(const OpaaxStringID InID, const OpaaxString& InParentPath = OpaaxString())
            : IEditorTitleBarNode(InID, InParentPath)
        {
        }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** @return The child category / command under InID, or null. */
        EditorTitleBarCategory*    FindCategory(OpaaxStringID InID);
        EditorTitleBarCommandNode* FindCommand(OpaaxStringID InID);

    public:
        /**
         * Gets or creates the sub-menu named InID, so two modules naming the same menu share it.
         */
        EditorTitleBarCategory& SubCategory(OpaaxStringID InID);

        /**
         * Adds an entry that dispatches InCommand.
         * @param InLabel   Its text, and its identity within this category
         * @param InCommand The command tag. Not checked here: an unknown tag is reported when clicked
         * @return The new entry, so SetEnabled / SetChecked can be chained. A duplicate label keeps the
         *   first entry and warns
         */
        EditorTitleBarCommandNode& AddCommand(OpaaxStringID InLabel, const OpaaxTag& InCommand);

        /** Adds a separator below the entries so far. */
        void AddSeparator();

        /** Greys out the whole menu. Checked every frame; unset means always enabled. */
        EditorTitleBarCategory& SetEnabled(FMenuPredicate InPredicate);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorMenuNode interface
        void   Draw(EditorContext& InContext) const override;
        Uint64 CountCommands() const noexcept override;

        EditorTitleBarCategory* AsCategory() noexcept override { return this; }
        //~End IEditorMenuNode interface

        // =============================================================================
        // Getter

        /** The children, in registration order. */
        const TDynArray<TUniquePtr<IEditorTitleBarNode>>& GetChildren() const noexcept { return m_Children; }

        bool IsEmpty() const noexcept { return m_Children.empty(); }

        // End Getter
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TUniquePtr<IEditorTitleBarNode>> m_Children;
        FMenuPredicate                         m_IsEnabled;
    };
}
