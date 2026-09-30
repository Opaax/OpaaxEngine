#pragma once

#include "Core/Tag/OpaaxTag.h"
#include "Editor/Commands/EditorCommandParams.h"
#include "Editor/Menus/IEditorTitleBarNode.h"

namespace Opaax::Editor
{
    /**
     * @class EditorTitleBarCommandNode
     *
     * A clickable entry. Carries only a command tag: clicking it runs Commands().Execute(tag, context),
     * the same call a key binding makes. No closure form: behaviour lives in commands.
     * All facets are optional: SetEnabled and SetChecked are predicates checked every frame,
     * SetParams is the payload.
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
         * Greys the entry out instead of letting it be clicked and refused.
         * @param InPredicate Checked every frame. Unset means always enabled
         * @return This, so facets can be chained
         */
        EditorTitleBarCommandNode& SetEnabled(FMenuPredicate InPredicate);

        /**
         * Makes the entry checkable, with InPredicate giving its tick.
         */
        EditorTitleBarCommandNode& SetChecked(FMenuPredicate InPredicate);

        /**
         * Draws this entry with a label computed at draw time ("Undo Move", not "Undo"). The id stays its
         * identity.
         */
        EditorTitleBarCommandNode& SetLabel(FMenuLabel InLabel);

        /**
         * The payload this entry dispatches with. Unset means NoParams. Keep it plain data (a key binding
         * could carry it too).
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
        FMenuLabel     m_Label;

        // Null means NoParams.
        TUniquePtr<IEditorCommandParams> m_Params;
    };
}
