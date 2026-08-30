#include "Editor/Menus/EditorMenuSeparatorNode.h"

#include "Editor/EditorContext.h"
#include "Editor/UI/IEditorGui.h"

namespace Opaax::Editor
{
    void EditorMenuSeparatorNode::Draw(EditorContext& InContext) const
    {
        InContext.Gui.MenuSeparator();
    }
}
