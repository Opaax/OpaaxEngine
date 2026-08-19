#include "Editor/Menus/EditorMenuCommandNode.h"

#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"

#include <imgui.h>

using namespace Opaax; // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace Opaax::Editor
{
    EditorMenuCommandNode& EditorMenuCommandNode::SetEnabled(FMenuPredicate InPredicate)
    {
        m_IsEnabled = Move(InPredicate);
        return *this;
    }

    EditorMenuCommandNode& EditorMenuCommandNode::SetChecked(FMenuPredicate InPredicate)
    {
        m_IsChecked = Move(InPredicate);
        return *this;
    }

    void EditorMenuCommandNode::Draw(EditorContext& InContext) const
    {
        const bool bEnabled = !m_IsEnabled || m_IsEnabled(InContext);
        const bool bChecked = m_IsChecked && m_IsChecked(InContext);

        if (!ImGui::MenuItem(GetLabel(), nullptr, bChecked, bEnabled))
        {
            return;
        }

        // EVERY entry announces itself, from the one place they are all invoked — a command added
        // later cannot forget to. The path is what a reader recognises, the tag what the registry
        // was asked for, so a miss says which of the two was wrong.
        OPAAX_LOG(LogEditorMenu, Info, "Menu: '{}' -> {}", GetPath().CStr(), m_Command);

        InContext.Extensions.Commands().Execute(m_Command, InContext);
    }
}
