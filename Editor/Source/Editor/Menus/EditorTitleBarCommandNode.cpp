#include "Editor/Menus/EditorTitleBarCommandNode.h"

#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/UI/IEditorGui.h"

using namespace Opaax;

namespace Opaax::Editor
{
    EditorTitleBarCommandNode& EditorTitleBarCommandNode::SetEnabled(FMenuPredicate InPredicate)
    {
        m_IsEnabled = Move(InPredicate);
        return *this;
    }

    EditorTitleBarCommandNode& EditorTitleBarCommandNode::SetChecked(FMenuPredicate InPredicate)
    {
        m_IsChecked = Move(InPredicate);
        return *this;
    }

    EditorTitleBarCommandNode& EditorTitleBarCommandNode::SetLabel(FMenuLabel InLabel)
    {
        m_Label = Move(InLabel);
        return *this;
    }

    void EditorTitleBarCommandNode::Draw(EditorContext& InContext) const
    {
        const bool bEnabled = !m_IsEnabled || m_IsEnabled(InContext);
        const bool bChecked = m_IsChecked && m_IsChecked(InContext);

        // Kept in a local for the call: MenuItem borrows the pointer, and a computed label is temporary.
        const OpaaxString lComputed = m_Label ? m_Label(InContext) : OpaaxString();

        if (!InContext.Gui.MenuItem(m_Label ? lComputed.CStr() : GetLabel(), bChecked, bEnabled))
        {
            return;
        }

        // Every entry is logged here, with its path and its tag.
        OPAAX_LOG(LogEditorMenu, Info, "Menu: '{}' -> {}", GetPath().CStr(), m_Command);

        const EditorCommandRegistry& lCommands = InContext.Extensions.Commands();

        if (m_Params != nullptr) { m_Params->Dispatch(lCommands, m_Command, InContext); }
        else                     { lCommands.Execute(m_Command, InContext); }
    }
}
