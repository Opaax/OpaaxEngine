#pragma once

#include "Editor/Menus/IEditorTitleBarNode.h"

namespace Opaax::Editor
{
    /**
     * @class EditorTitleBarSeparatorNode
     *
     * A rule between entries. A node rather than a flag on the entry below it, because a separator
     * is POSITIONAL: it lives in the same ordered child list as everything else, so where it was
     * written is where it draws — and a category can end up with one at either end or none at all
     * without any entry having to know.
     *
     * Its id is invalid: there is nothing to label and nothing to look it up by.
     */
    class EditorTitleBarSeparatorNode final : public IEditorTitleBarNode
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit EditorTitleBarSeparatorNode(const OpaaxString& InParentPath)
            : IEditorTitleBarNode(OpaaxStringID(), InParentPath)
        {
        }

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorMenuNode interface
        void   Draw(EditorContext& InContext) const override;
        Uint64 CountCommands() const noexcept override { return 0; }
        //~End IEditorMenuNode interface
    };
}
