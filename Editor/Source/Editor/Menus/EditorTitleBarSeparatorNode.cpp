#include "Editor/Menus/EditorTitleBarSeparatorNode.h"

#include "Editor/EditorContext.h"
#include "Editor/UI/IEditorGui.h"

namespace Opaax::Editor
{
    void EditorTitleBarSeparatorNode::Draw(EditorContext& InContext) const
    {
        InContext.Gui.MenuSeparator();
    }
}
