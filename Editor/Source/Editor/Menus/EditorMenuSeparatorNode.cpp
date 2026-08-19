#include "Editor/Menus/EditorMenuSeparatorNode.h"

#include <imgui.h>

namespace Opaax::Editor
{
    void EditorMenuSeparatorNode::Draw(EditorContext&) const
    {
        ImGui::Separator();
    }
}
