#pragma once

#include "Editor/Menus/IEditorTitleBarNode.h"

namespace Opaax::Editor
{
    /**
     * @class EditorTitleBarSeparatorNode
     *
     * A separator between entries. A node in the ordered child list, so it draws where it was added.
     * Its id is invalid.
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
